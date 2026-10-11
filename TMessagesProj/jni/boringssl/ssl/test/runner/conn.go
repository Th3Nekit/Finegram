// Copyright 2010 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// TLS low level connection and record layer

package runner

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdsa"
	"crypto/subtle"
	"crypto/x509"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"slices"
	"sync"
	"time"

	"golang.org/x/crypto/chacha20"
	"golang.org/x/crypto/cryptobyte"
)

type dtlsRecordInfo struct {
	typ   recordType
	epoch uint16

	bytesAvailable int
}

type Conn struct {

	conn     net.Conn
	isDTLS   bool
	isClient bool

	handshakeMutex          sync.Mutex
	handshakeErr            error
	wireVersion             uint16
	vers                    uint16
	haveVers                bool
	config                  *Config
	handshakeComplete       bool
	skipEarlyData           bool
	didResume               bool
	extendedMasterSecret    bool
	cipherSuite             *cipherSuite
	ocspResponse            []byte
	sctList                 []byte
	peerCertificates        []*x509.Certificate
	peerDelegatedCredential []byte

	verifiedChains [][]*x509.Certificate

	serverName string

	firstFinished [12]byte

	peerSignatureAlgorithm signatureAlgorithm

	curveID CurveID

	quicTransportParams []byte

	quicTransportParamsLegacy []byte

	clientRandom, serverRandom [32]byte
	earlyExporterSecret        []byte
	exporterSecret             []byte
	resumptionSecret           []byte

	clientProtocol         string
	clientProtocolFallback bool
	usedALPN               bool

	localApplicationSettings, peerApplicationSettings       []byte
	hasApplicationSettings                                  bool
	localApplicationSettingsOld, peerApplicationSettingsOld []byte
	hasApplicationSettingsOld                               bool

	clientVerify []byte
	serverVerify []byte

	channelID *ecdsa.PublicKey

	srtpProtectionProfile uint16

	clientVersion uint16

	in, out  halfConn
	rawInput bytes.Buffer
	input    bytes.Buffer
	hand     bytes.Buffer

	pendingFlight bytes.Buffer

	sendHandshakeSeq uint16
	recvHandshakeSeq uint16
	handMsg          []byte
	handMsgLen       int
	pendingPacket    []byte
	maxPacketLen     int

	previousFlight        []DTLSMessage
	receivedFlight        []DTLSMessage
	receivedFlightRecords []DTLSRecordNumberInfo
	nextFlight            []DTLSMessage
	expectedACK           []DTLSRecordNumber

	keyUpdateSeen      bool
	keyUpdateRequested bool
	seenOneByteRecord  bool

	expectTLS13ChangeCipherSpec bool

	seenHandshakePackEnd bool

	lastRecordInFlight *dtlsRecordInfo

	bytesAvailableInPacket int

	skipRecordVersionCheck bool

	echAccepted bool

	tmp [16]byte
}

func (c *Conn) init() {
	c.in.isDTLS = c.isDTLS
	c.out.isDTLS = c.isDTLS
	c.in.config = c.config
	c.out.config = c.config
	c.in.conn = c
	c.out.conn = c
	c.maxPacketLen = c.config.Bugs.MaxPacketLength
}

func (c *Conn) LocalAddr() net.Addr {
	return c.conn.LocalAddr()
}

func (c *Conn) RemoteAddr() net.Addr {
	return c.conn.RemoteAddr()
}

func (c *Conn) SetDeadline(t time.Time) error {
	return c.conn.SetDeadline(t)
}

func (c *Conn) SetReadDeadline(t time.Time) error {
	return c.conn.SetReadDeadline(t)
}

func (c *Conn) SetWriteDeadline(t time.Time) error {
	return c.conn.SetWriteDeadline(t)
}

const maxEpochs = 4

type epochState struct {
	epoch                 uint16
	cipher                any
	recordNumberEncrypter recordNumberEncrypter
	mac                   macFunction
	seq                   [8]byte
}

type halfConn struct {
	sync.Mutex

	err         error
	version     uint16
	wireVersion uint16
	isDTLS      bool
	epoch       epochState
	pastEpochs  []epochState

	nextEpoch epochState

	macBuf []byte

	trafficSecret []byte

	config *Config
	conn   *Conn
}

func (hc *halfConn) setErrorLocked(err error) error {
	hc.err = err
	return err
}

func (hc *halfConn) error() error {

	err := hc.err
	return err
}

func (hc *halfConn) getEpoch(epochValue uint16) (*epochState, bool) {
	if hc.epoch.epoch == epochValue {
		return &hc.epoch, true
	}
	for i := range hc.pastEpochs {
		if hc.pastEpochs[i].epoch == epochValue {
			return &hc.pastEpochs[i], true
		}
	}
	return nil, false
}

func (hc *halfConn) changeEpoch(epoch epochState) {
	if len(hc.pastEpochs) < maxEpochs {
		hc.pastEpochs = append(hc.pastEpochs, hc.epoch)
	} else {
		for i := 1; i < len(hc.pastEpochs); i++ {
			hc.pastEpochs[i-1] = hc.pastEpochs[i]
		}
		hc.pastEpochs[len(hc.pastEpochs)-1] = hc.epoch
	}
	hc.epoch = epoch
}

func (hc *halfConn) newEpochState(epoch uint16, cipher any, mac macFunction) epochState {
	ret := epochState{epoch: epoch, cipher: cipher, mac: mac}
	if hc.isDTLS {
		binary.BigEndian.PutUint16(ret.seq[:2], epoch)
	}
	return ret
}

func (hc *halfConn) prepareCipherSpec(version uint16, cipher any, mac macFunction) {
	hc.wireVersion = version
	protocolVersion, ok := wireToVersion(version, hc.isDTLS)
	if !ok {
		panic("TLS: unknown version")
	}
	hc.version = protocolVersion
	epoch := hc.epoch.epoch + 1
	if epoch == 0 {
		panic("TLS: epoch overflow")
	}
	hc.nextEpoch = hc.newEpochState(epoch, cipher, mac)
}

func (hc *halfConn) changeCipherSpec() error {
	if hc.nextEpoch.cipher == nil {
		return alertInternalError
	}
	hc.changeEpoch(hc.nextEpoch)
	hc.nextEpoch = epochState{}

	if hc.config.Bugs.NullAllCiphers {
		hc.epoch.cipher = nullCipher{}
		hc.epoch.mac = nil
	}
	return nil
}

func (hc *halfConn) useTrafficSecret(version uint16, suite *cipherSuite, secret []byte, side trafficDirection, epoch uint16) {
	hc.wireVersion = version
	protocolVersion, ok := wireToVersion(version, hc.isDTLS)
	if !ok {
		panic("TLS: unknown version")
	}
	hc.version = protocolVersion
	newEpoch := hc.newEpochState(epoch, deriveTrafficAEAD(version, suite, secret, side, hc.isDTLS), nil)
	if hc.isDTLS && !hc.config.Bugs.NullAllCiphers {
		sn_key := hkdfExpandLabel(suite.hash(), secret, []byte("sn"), nil, suite.keyLen, hc.isDTLS)
		switch suite.id {
		case TLS_CHACHA20_POLY1305_SHA256:
			newEpoch.recordNumberEncrypter = newChachaRecordNumberEncrypter(sn_key)
		case TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384:
			newEpoch.recordNumberEncrypter = newAESRecordNumberEncrypter(sn_key)
		default:
			panic("Cipher suite does not support TLS 1.3")
		}
	}
	if hc.config.Bugs.NullAllCiphers {
		newEpoch.cipher = nullCipher{}
	}
	hc.trafficSecret = secret
	hc.changeEpoch(newEpoch)
}

func (hc *halfConn) resetCipher() {
	initialEpoch, ok := hc.getEpoch(0)
	if !ok {
		panic("tls: could not find initial epoch")
	}
	hc.epoch = *initialEpoch
	hc.pastEpochs = nil
}

func (hc *halfConn) incSeq(epoch *epochState) {
	limit := 0
	increment := uint64(1)
	if hc.isDTLS {

		limit = 2
	}
	for i := 7; i >= limit; i-- {
		increment += uint64(epoch.seq[i])
		epoch.seq[i] = byte(increment)
		increment >>= 8
	}

	if increment != 0 {
		panic("TLS: sequence number wraparound")
	}
}

func (hc *halfConn) lastRecordNumber(epoch *epochState, isOut bool) DTLSRecordNumber {
	seq := binary.BigEndian.Uint64(epoch.seq[:])

	if seq&(1<<48-1) == 0 {
		panic("tls: epoch has never been used")
	}
	seq--
	if hc.isDTLS {
		if isOut && hc.config.Bugs.SequenceNumberMapping != nil {
			seq = hc.config.Bugs.SequenceNumberMapping(seq)
		}

		seq &= 1<<48 - 1
	}
	return DTLSRecordNumber{Epoch: uint64(epoch.epoch), Sequence: seq}
}

func (hc *halfConn) sequenceNumberForOutput(epoch *epochState) []byte {
	if !hc.isDTLS || hc.config.Bugs.SequenceNumberMapping == nil {
		return epoch.seq[:]
	}

	var seq [8]byte
	seqU64 := binary.BigEndian.Uint64(epoch.seq[:])
	seqU64 = hc.config.Bugs.SequenceNumberMapping(seqU64)
	binary.BigEndian.PutUint64(seq[:], seqU64)

	copy(seq[:2], epoch.seq[:2])
	return seq[:]
}

func (hc *halfConn) explicitIVLen(epoch *epochState) int {
	if epoch.cipher == nil {
		return 0
	}
	switch c := epoch.cipher.(type) {
	case cipher.Stream:
		return 0
	case *tlsAead:
		if c.explicitNonce {
			return 8
		}
		return 0
	case *cbcMode:
		if hc.version >= VersionTLS11 || hc.isDTLS {
			return c.BlockSize()
		}
		return 0
	case nullCipher:
		return 0
	default:
		panic("unknown cipher type")
	}
}

func (hc *halfConn) computeMAC(epoch *epochState, seq, header, data []byte) []byte {
	hc.macBuf = epoch.mac.MAC(hc.macBuf[:0], seq, header[:3], header[len(header)-2:], data)
	return hc.macBuf
}

func removePadding(payload []byte) ([]byte, byte) {
	if len(payload) < 1 {
		return payload, 0
	}

	paddingLen := payload[len(payload)-1]
	t := uint(len(payload)-1) - uint(paddingLen)

	good := byte(int32(^t) >> 31)

	toCheck := 255

	if toCheck+1 > len(payload) {
		toCheck = len(payload) - 1
	}

	for i := 0; i < toCheck; i++ {
		t := uint(paddingLen) - uint(i)

		mask := byte(int32(^t) >> 31)
		b := payload[len(payload)-1-i]
		good &^= mask&paddingLen ^ mask&b
	}

	good &= good << 4
	good &= good << 2
	good &= good << 1
	good = uint8(int8(good) >> 7)

	toRemove := good&paddingLen + 1
	return payload[:len(payload)-int(toRemove)], good
}

func roundUp(a, b int) int {
	return a + (b-a%b)%b
}

func (hc *halfConn) decrypt(epoch *epochState, recordHeaderLen int, record []byte) (ok bool, contentType recordType, data []byte, alertValue alert) {

	payload := record[recordHeaderLen:]

	macSize := 0
	if epoch.mac != nil {
		macSize = epoch.mac.Size()
	}

	paddingGood := byte(255)
	explicitIVLen := hc.explicitIVLen(epoch)

	if epoch.cipher != nil {
		switch c := epoch.cipher.(type) {
		case cipher.Stream:
			c.XORKeyStream(payload, payload)
		case *tlsAead:
			nonce := epoch.seq[:]
			if hc.isDTLS && hc.version >= VersionTLS13 && !hc.conn.useDTLSPlaintextHeader() {

				nonce = make([]byte, 8)
				copy(nonce[2:], epoch.seq[2:])
			}

			if explicitIVLen != 0 {
				if len(payload) < explicitIVLen {
					return false, 0, nil, alertBadRecordMAC
				}
				nonce = payload[:explicitIVLen]
				payload = payload[explicitIVLen:]
			}

			var additionalData []byte
			if hc.version < VersionTLS13 {
				additionalData = make([]byte, 13)
				copy(additionalData, epoch.seq[:])
				copy(additionalData[8:], record[:3])
				n := len(payload) - c.Overhead()
				additionalData[11] = byte(n >> 8)
				additionalData[12] = byte(n)
			} else {
				additionalData = record[:recordHeaderLen]
			}
			var err error
			payload, err = c.Open(payload[:0], nonce, payload, additionalData)
			if err != nil {
				return false, 0, nil, alertBadRecordMAC
			}
		case *cbcMode:
			blockSize := c.BlockSize()
			if len(payload)%blockSize != 0 || len(payload) < roundUp(explicitIVLen+macSize+1, blockSize) {
				return false, 0, nil, alertBadRecordMAC
			}

			if explicitIVLen > 0 {
				c.SetIV(payload[:explicitIVLen])
				payload = payload[explicitIVLen:]
			}
			c.CryptBlocks(payload, payload)
			payload, paddingGood = removePadding(payload)

		case nullCipher:
			break
		default:
			panic("unknown cipher type")
		}

		if hc.version >= VersionTLS13 {
			i := len(payload)
			for i > 0 && payload[i-1] == 0 {
				i--
			}
			payload = payload[:i]
			if len(payload) == 0 {
				return false, 0, nil, alertUnexpectedMessage
			}
			contentType = recordType(payload[len(payload)-1])
			payload = payload[:len(payload)-1]
		}
	}

	if epoch.mac != nil {
		if len(payload) < macSize {
			return false, 0, nil, alertBadRecordMAC
		}

		n := len(payload) - macSize
		remoteMAC := payload[n:]
		payload = payload[:n]
		record[recordHeaderLen-2] = byte(n >> 8)
		record[recordHeaderLen-1] = byte(n)
		localMAC := hc.computeMAC(epoch, epoch.seq[:], record[:recordHeaderLen], payload)
		if subtle.ConstantTimeCompare(localMAC, remoteMAC) != 1 || paddingGood != 255 {
			return false, 0, nil, alertBadRecordMAC
		}
	}
	hc.incSeq(epoch)

	return true, contentType, payload, 0
}

func extendSlice(data *[]byte, n int) []byte {

	*data = slices.Grow(*data, n)

	oldLen := len(*data)
	newLen := oldLen + n
	*data = (*data)[:newLen]
	return (*data)[oldLen:newLen]
}

func computingCBCPaddingLength(payloadLen, blockSize int, config *Config) int {
	paddingLen := blockSize - payloadLen%blockSize
	if config.Bugs.MaxPadding {
		for paddingLen+blockSize <= 256 {
			paddingLen += blockSize
		}
	}
	return paddingLen
}

func appendCBCPadding(b []byte, paddingLen int, config *Config) []byte {
	padding := extendSlice(&b, paddingLen)
	for i := range padding {
		padding[i] = byte(paddingLen - 1)
	}
	if config.Bugs.PaddingFirstByteBad || config.Bugs.PaddingFirstByteBadIf255 && paddingLen == 256 {
		padding[0] ^= 0xff
	}
	return b
}

func (hc *halfConn) maxEncryptOverhead(epoch *epochState, payloadLen int) int {
	var macSize int
	if epoch.mac != nil {
		macSize = epoch.mac.Size()
	}
	overhead := macSize + hc.explicitIVLen(epoch)
	if hc.version >= VersionTLS13 {
		overhead += 1 + hc.config.Bugs.RecordPadding
	}
	if epoch.cipher != nil {
		switch c := epoch.cipher.(type) {
		case cipher.Stream, *nullCipher:
		case *tlsAead:
			overhead += c.Overhead()
		case *cbcMode:
			overhead += computingCBCPaddingLength(payloadLen+macSize, c.BlockSize(), hc.config)
		case nullCipher:
			break
		default:
			panic("unknown cipher type")
		}
	}
	return overhead
}

func (c *Conn) useDTLSPlaintextHeader() bool {
	return c.config.Bugs.DTLSUsePlaintextRecordHeader && c.handshakeComplete
}

func (hc *halfConn) encrypt(epoch *epochState, record, payload []byte, typ recordType, headerLen int, headerHasLength bool) ([]byte, error) {
	seq := hc.sequenceNumberForOutput(epoch)
	prefixLen := len(record)
	header := record[prefixLen-headerLen:]
	explicitIVLen := hc.explicitIVLen(epoch)

	extendSlice(&record, explicitIVLen)

	record = append(record, payload...)

	if hc.version >= VersionTLS13 && epoch.cipher != nil {
		if hc.config.Bugs.OmitRecordContents {
			record = record[:len(record)-len(payload)]
		} else {
			record = append(record, byte(typ))
		}
		padding := extendSlice(&record, hc.config.Bugs.RecordPadding)
		clear(padding)
	}

	if epoch.mac != nil {
		record = append(record, hc.computeMAC(epoch, seq, header, payload)...)
	}

	explicitIV := record[prefixLen : prefixLen+explicitIVLen]
	if epoch.cipher != nil {
		switch c := epoch.cipher.(type) {
		case cipher.Stream:
			if explicitIVLen != 0 {
				panic("tls: unexpected explicit IV length")
			}
			c.XORKeyStream(record[prefixLen:], record[prefixLen:])
		case *tlsAead:
			nonce := seq
			if hc.isDTLS && hc.version >= VersionTLS13 && !hc.conn.useDTLSPlaintextHeader() {

				nonce = make([]byte, 8)
				copy(nonce[2:], seq[2:])
			}

			if len(explicitIV) != 0 {
				if explicitIVLen != len(nonce) {
					panic("tls: unexpected explicit IV length")
				}
				copy(explicitIV, nonce)
			}

			var additionalData []byte
			if hc.version < VersionTLS13 {

				additionalData = make([]byte, 13)
				copy(additionalData, seq)
				copy(additionalData[8:], header[:3])
				additionalData[11] = byte(len(payload) >> 8)
				additionalData[12] = byte(len(payload))
			} else {

				if headerHasLength {
					n := len(record) - prefixLen + c.Overhead()
					record[prefixLen-2] = byte(n >> 8)
					record[prefixLen-1] = byte(n)
				}
				additionalData = record[prefixLen-headerLen : prefixLen]
			}

			record = c.Seal(record[:prefixLen+explicitIVLen], nonce, record[prefixLen+explicitIVLen:], additionalData)
		case *cbcMode:
			if explicitIVLen > 0 {
				if _, err := io.ReadFull(hc.config.rand(), explicitIV); err != nil {
					return nil, err
				}
				c.SetIV(explicitIV)
			}

			blockSize := c.BlockSize()
			paddingLen := computingCBCPaddingLength(len(record)-prefixLen, blockSize, hc.config)
			record = appendCBCPadding(record, paddingLen, hc.config)
			c.CryptBlocks(record[prefixLen:], record[prefixLen:])
		case nullCipher:
			break
		default:
			panic("unknown cipher type")
		}
	}

	if headerHasLength {
		n := len(record) - prefixLen
		record[prefixLen-2] = byte(n >> 8)
		record[prefixLen-1] = byte(n)
	}
	hc.incSeq(epoch)

	return record, nil
}

type recordNumberEncrypter interface {

	generateMask(sample []byte) []byte
}

type aesRecordNumberEncrypter struct {
	aesCipher cipher.Block
}

func newAESRecordNumberEncrypter(key []byte) *aesRecordNumberEncrypter {
	aesCipher, err := aes.NewCipher(key)
	if err != nil {
		panic("Incorrect usage of newAESRecordNumberEncrypter")
	}
	return &aesRecordNumberEncrypter{
		aesCipher: aesCipher,
	}
}

func (a *aesRecordNumberEncrypter) generateMask(sample []byte) []byte {
	out := make([]byte, len(sample))
	a.aesCipher.Encrypt(out, sample)
	return out
}

type chachaRecordNumberEncrypter struct {
	key []byte
}

func newChachaRecordNumberEncrypter(key []byte) *chachaRecordNumberEncrypter {
	out := &chachaRecordNumberEncrypter{
		key: key,
	}
	return out
}

func (c *chachaRecordNumberEncrypter) generateMask(sample []byte) []byte {
	var counter, nonce []byte
	sampleReader := cryptobyte.String(sample)
	if !sampleReader.ReadBytes(&counter, 4) || !sampleReader.ReadBytes(&nonce, 12) {
		panic("chachaRecordNumberEncrypter.GenerateMask called with wrong size sample")
	}
	cipher, err := chacha20.NewUnauthenticatedCipher(c.key, nonce)
	if err != nil {
		panic("Failed to create chacha20 cipher for record number encryption")
	}
	cipher.SetCounter(binary.LittleEndian.Uint32(counter))
	out := make([]byte, 2)
	cipher.XORKeyStream(out, out)
	return out
}

func (c *Conn) useInTrafficSecret(epoch uint16, version uint16, suite *cipherSuite, secret []byte) error {
	if c.hand.Len() != 0 {
		return c.in.setErrorLocked(errors.New("tls: buffered handshake messages on cipher change"))
	}
	side := serverWrite
	if !c.isClient {
		side = clientWrite
	}
	if c.config.Bugs.MockQUICTransport != nil {
		if epoch > uint16(encryptionApplication) {
			panic("tls: KeyUpdate processed in QUIC")
		}
		c.config.Bugs.MockQUICTransport.readLevel = encryptionLevel(epoch)
		c.config.Bugs.MockQUICTransport.readSecret = secret
		c.config.Bugs.MockQUICTransport.readCipherSuite = suite.id
	}
	c.in.useTrafficSecret(version, suite, secret, side, epoch)
	c.seenHandshakePackEnd = false
	return nil
}

func (c *Conn) useOutTrafficSecret(epoch uint16, version uint16, suite *cipherSuite, secret []byte) {
	if !c.isDTLS {

		c.flushHandshake()
	}
	side := serverWrite
	if c.isClient {
		side = clientWrite
	}
	if c.config.Bugs.MockQUICTransport != nil {
		if epoch > uint16(encryptionApplication) {
			panic("tls: KeyUpdate processed in QUIC")
		}
		c.config.Bugs.MockQUICTransport.writeLevel = encryptionLevel(epoch)
		c.config.Bugs.MockQUICTransport.writeSecret = secret
		c.config.Bugs.MockQUICTransport.writeCipherSuite = suite.id
	}
	c.out.useTrafficSecret(version, suite, secret, side, epoch)
}

func (c *Conn) setSkipEarlyData() {
	if c.config.Bugs.MockQUICTransport != nil {
		c.config.Bugs.MockQUICTransport.skipEarlyData = true
	} else {
		c.skipEarlyData = true
	}
}

func (c *Conn) shouldSkipEarlyData() bool {
	if c.config.Bugs.MockQUICTransport != nil {
		return c.config.Bugs.MockQUICTransport.skipEarlyData
	}
	return c.skipEarlyData
}

func (c *Conn) readRawInputUntil(n int) error {
	if c.rawInput.Len() >= n {
		return nil
	}

	n -= c.rawInput.Len()
	c.rawInput.Grow(n)
	buf := c.rawInput.AvailableBuffer()
	nread, err := io.ReadAtLeast(c.conn, buf[:cap(buf)], n)
	c.rawInput.Write(buf[:nread])
	return err
}

func (c *Conn) doReadRecord(want recordType) (recordType, []byte, error) {
RestartReadRecord:
	if c.isDTLS {
		return c.dtlsDoReadRecord(&c.in.epoch, want)
	}

	recordHeaderLen := tlsRecordHeaderLen

	if err := c.readRawInputUntil(recordHeaderLen); err != nil {

		if err == io.EOF && c.config.Bugs.ExpectCloseNotify {
			err = io.ErrUnexpectedEOF
		}
		if e, ok := err.(net.Error); !ok || !e.Temporary() {
			c.in.setErrorLocked(err)
		}
		return 0, nil, err
	}

	header := c.rawInput.Bytes()[:recordHeaderLen]
	typ := recordType(header[0])

	if want == recordTypeHandshake && typ == 0x80 {
		c.sendAlert(alertProtocolVersion)
		return 0, nil, c.in.setErrorLocked(errors.New("tls: unsupported SSLv2 handshake received"))
	}

	vers := uint16(header[1])<<8 | uint16(header[2])
	n := int(header[3])<<8 | int(header[4])

	if typ != recordTypeAlert {
		var expect uint16
		if c.haveVers {
			expect = c.vers
			if c.vers >= VersionTLS13 {
				expect = VersionTLS12
			}
		} else {
			expect = c.config.Bugs.ExpectInitialRecordVersion
		}
		if expect != 0 && vers != expect {
			c.sendAlert(alertProtocolVersion)
			return 0, nil, c.in.setErrorLocked(fmt.Errorf("tls: received record with version %x when expecting version %x", vers, expect))
		}
	}
	if n > maxCiphertext {
		c.sendAlert(alertRecordOverflow)
		return 0, nil, c.in.setErrorLocked(fmt.Errorf("tls: oversized record received with length %d", n))
	}
	if !c.haveVers {

		if (typ != recordTypeAlert && typ != want) || vers >= 0x1000 || n >= 0x3000 {
			c.sendAlert(alertUnexpectedMessage)
			return 0, nil, c.in.setErrorLocked(fmt.Errorf("tls: first record does not look like a TLS handshake"))
		}
	}
	if err := c.readRawInputUntil(recordHeaderLen + n); err != nil {
		if err == io.EOF {
			err = io.ErrUnexpectedEOF
		}
		if e, ok := err.(net.Error); !ok || !e.Temporary() {
			c.in.setErrorLocked(err)
		}
		return 0, nil, err
	}

	b := c.rawInput.Next(recordHeaderLen + n)
	epoch := &c.in.epoch
	ok, encTyp, data, alertValue := c.in.decrypt(epoch, recordHeaderLen, b)
	if !ok {

		if c.skipEarlyData {
			goto RestartReadRecord
		}
		return 0, nil, c.in.setErrorLocked(c.sendAlert(alertValue))
	}

	if epoch.cipher == nil && typ == recordTypeApplicationData && c.skipEarlyData {
		goto RestartReadRecord
	}

	c.skipEarlyData = false

	if c.vers >= VersionTLS13 && epoch.cipher != nil {
		if typ != recordTypeApplicationData {
			return 0, nil, c.in.setErrorLocked(fmt.Errorf("tls: outer record type is not application data"))
		}
		typ = encTyp
	}

	if c.config.Bugs.ExpectRecordSplitting && typ == recordTypeApplicationData && len(data) != 1 && !c.seenOneByteRecord {
		return 0, nil, c.in.setErrorLocked(fmt.Errorf("tls: application data records were not split"))
	}

	c.seenOneByteRecord = typ == recordTypeApplicationData && len(data) == 1
	return typ, data, nil
}

func (c *Conn) readTLS13ChangeCipherSpec() error {
	if c.config.Bugs.MockQUICTransport != nil {
		return nil
	}
	if c.isDTLS {

		return nil
	}
	if !c.expectTLS13ChangeCipherSpec {
		panic("c.expectTLS13ChangeCipherSpec not set")
	}

	if err := c.readRawInputUntil(6); err != nil {
		return c.in.setErrorLocked(fmt.Errorf("tls: error reading TLS 1.3 ChangeCipherSpec: %s", err))
	}
	if recordType(c.rawInput.Bytes()[0]) == recordTypeAlert {

		c.expectTLS13ChangeCipherSpec = false
		return nil
	}

	expected := [6]byte{byte(recordTypeChangeCipherSpec), 3, 1, 0, 1, 1}
	if c.vers >= VersionTLS13 {
		expected[2] = 3
	}
	if data := c.rawInput.Bytes()[:6]; !bytes.Equal(data, expected[:]) {
		return c.in.setErrorLocked(fmt.Errorf("tls: error invalid TLS 1.3 ChangeCipherSpec: %x", data))
	}

	c.rawInput.Next(6)

	c.expectTLS13ChangeCipherSpec = false
	return nil
}

func (c *Conn) readRecord(want recordType) error {

	switch want {
	default:
		c.sendAlert(alertInternalError)
		return c.in.setErrorLocked(errors.New("tls: unknown record type requested"))
	case recordTypeChangeCipherSpec:
		if c.handshakeComplete {
			c.sendAlert(alertInternalError)
			return c.in.setErrorLocked(errors.New("tls: ChangeCipherSpec requested after handshake complete"))
		}
	case recordTypeApplicationData, recordTypeAlert, recordTypeHandshake, recordTypeACK:
		break
	}

	if c.expectTLS13ChangeCipherSpec {
		if err := c.readTLS13ChangeCipherSpec(); err != nil {
			return err
		}
	}

Again:
	doReadRecord := c.doReadRecord
	if c.config.Bugs.MockQUICTransport != nil {
		doReadRecord = c.config.Bugs.MockQUICTransport.readRecord
	}
	typ, data, err := doReadRecord(want)
	if err != nil {
		return err
	}
	max := maxPlaintext
	if c.config.Bugs.MaxReceivePlaintext != 0 {
		max = c.config.Bugs.MaxReceivePlaintext
	}
	if len(data) > max {
		err := c.sendAlert(alertRecordOverflow)
		return c.in.setErrorLocked(err)
	}

	if typ != recordTypeHandshake {
		c.seenHandshakePackEnd = false
	} else if c.seenHandshakePackEnd {
		return c.in.setErrorLocked(errors.New("tls: peer violated ExpectPackedEncryptedHandshake"))
	}

	switch typ {
	default:
		c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))

	case recordTypeAlert:
		if len(data) != 2 {
			c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))
			break
		}
		if alert(data[1]) == alertCloseNotify {
			c.in.setErrorLocked(io.EOF)
			break
		}
		switch data[0] {
		case alertLevelWarning:

			goto Again
		case alertLevelError:
			c.in.setErrorLocked(&net.OpError{Op: "remote error", Err: alert(data[1])})
		default:
			c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))
		}

	case recordTypeChangeCipherSpec:
		if typ != want || len(data) != 1 || data[0] != 1 {
			c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))
			break
		}
		if c.hand.Len() != 0 {
			c.in.setErrorLocked(errors.New("tls: buffered handshake messages on cipher change"))
			break
		}
		if c.isDTLS {

			c.receivedFlight = append(c.receivedFlight, DTLSMessage{
				Epoch:              c.in.epoch.epoch,
				IsChangeCipherSpec: true,
				Data:               slices.Clone(data),
			})
		}
		if err := c.in.changeCipherSpec(); err != nil {
			c.in.setErrorLocked(c.sendAlert(err.(alert)))
		}

	case recordTypeApplicationData:
		if typ != want {
			c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))
			break
		}
		c.input.Write(data)

	case recordTypeHandshake:

		if typ != want && want != recordTypeApplicationData {
			return c.in.setErrorLocked(c.sendAlert(alertNoRenegotiation))
		}
		c.hand.Write(data)
		if pack := c.config.Bugs.ExpectPackedEncryptedHandshake; pack > 0 && len(data) < pack && c.out.epoch.cipher != nil {
			c.seenHandshakePackEnd = true
		}
		if c.isDTLS {
			record, err := c.makeDTLSRecordNumberInfo(&c.in.epoch, c.hand.Bytes())
			if err != nil {
				return err
			}
			c.receivedFlightRecords = append(c.receivedFlightRecords, record)
		}

	case recordTypeACK:
		if typ != want || !c.isDTLS {
			c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))
			break
		}

		if err := c.checkACK(data); err != nil {
			c.in.setErrorLocked(err)
			break
		}
	}

	return c.in.err
}

func (c *Conn) sendAlertLocked(level byte, err alert) error {
	c.tmp[0] = level
	c.tmp[1] = byte(err)
	if c.config.Bugs.FragmentAlert {
		c.writeRecord(recordTypeAlert, c.tmp[0:1])
		c.writeRecord(recordTypeAlert, c.tmp[1:2])
	} else if c.config.Bugs.DoubleAlert {
		copy(c.tmp[2:4], c.tmp[0:2])
		c.writeRecord(recordTypeAlert, c.tmp[0:4])
	} else {
		c.writeRecord(recordTypeAlert, c.tmp[0:2])
	}

	if level == alertLevelError {
		return c.out.setErrorLocked(&net.OpError{Op: "local error", Err: err})
	}
	return nil
}

func (c *Conn) sendAlert(err alert) error {
	level := byte(alertLevelError)
	if err == alertNoRenegotiation || err == alertCloseNotify {
		level = alertLevelWarning
	}
	return c.SendAlert(level, err)
}

func (c *Conn) SendAlert(level byte, err alert) error {
	c.out.Lock()
	defer c.out.Unlock()
	return c.sendAlertLocked(level, err)
}

func (c *Conn) writeV2Record(data []byte) (n int, err error) {
	record := make([]byte, 2+len(data))
	record[0] = uint8(len(data)>>8) | 0x80
	record[1] = uint8(len(data))
	copy(record[2:], data)
	return c.conn.Write(record)
}

func (c *Conn) writeRecord(typ recordType, data []byte) (n int, err error) {
	c.seenHandshakePackEnd = false
	if c.hand.Len() == 0 {
		c.lastRecordInFlight = nil
	}
	if typ == recordTypeHandshake {
		msgType := data[0]
		if c.config.Bugs.SendWrongMessageType != 0 && msgType == c.config.Bugs.SendWrongMessageType {
			msgType += 42
		}
		if msgType != data[0] {
			data = append([]byte{msgType}, data[1:]...)
		}

		if c.config.Bugs.SendTrailingMessageData != 0 && msgType == c.config.Bugs.SendTrailingMessageData {

			newData := make([]byte, len(data)+1)
			copy(newData, data)

			newLen := len(newData) - 4
			newData[1] = byte(newLen >> 16)
			newData[2] = byte(newLen >> 8)
			newData[3] = byte(newLen)

			data = newData
		}

		if c.config.Bugs.TrailingDataWithFinished && msgType == typeFinished {

			data = append(data[:len(data):len(data)], 0)
		}
	}

	if c.isDTLS {
		return c.dtlsWriteRecord(typ, data)
	}
	if c.config.Bugs.MockQUICTransport != nil {
		return c.config.Bugs.MockQUICTransport.writeRecord(typ, data)
	}

	if typ == recordTypeHandshake {
		if c.config.Bugs.SendHelloRequestBeforeEveryHandshakeMessage {
			newData := make([]byte, 0, 4+len(data))
			newData = append(newData, typeHelloRequest, 0, 0, 0)
			newData = append(newData, data...)
			data = newData
		}

		if c.config.Bugs.PackHandshakeFlight {
			c.pendingFlight.Write(data)
			return len(data), nil
		}
	}

	if err := c.flushHandshake(); err != nil {
		return 0, err
	}

	if typ == recordTypeApplicationData && c.config.Bugs.SendPostHandshakeChangeCipherSpec {
		if _, err := c.doWriteRecord(recordTypeChangeCipherSpec, []byte{1}); err != nil {
			return 0, err
		}
	}

	return c.doWriteRecord(typ, data)
}

func (c *Conn) doWriteRecord(typ recordType, data []byte) (n int, err error) {
	first := true
	for len(data) > 0 || first {
		m := len(data)
		if m > maxPlaintext && !c.config.Bugs.SendLargeRecords {
			m = maxPlaintext
		}
		if typ == recordTypeHandshake && c.config.Bugs.MaxHandshakeRecordLength > 0 && m > c.config.Bugs.MaxHandshakeRecordLength {
			m = c.config.Bugs.MaxHandshakeRecordLength
		}
		first = false

		vers := c.vers
		if vers == 0 {

			vers = VersionTLS10
		}
		if c.vers >= VersionTLS13 || c.out.version >= VersionTLS13 {
			vers = VersionTLS12
		}
		if c.config.Bugs.SendRecordVersion != 0 {
			vers = c.config.Bugs.SendRecordVersion
		}
		if c.vers == 0 && c.config.Bugs.SendInitialRecordVersion != 0 {
			vers = c.config.Bugs.SendInitialRecordVersion
		}

		epoch := &c.out.epoch
		record := make([]byte, tlsRecordHeaderLen, tlsRecordHeaderLen+m+c.out.maxEncryptOverhead(epoch, m))
		record[0] = byte(typ)
		if c.vers >= VersionTLS13 && epoch.cipher != nil {
			record[0] = byte(recordTypeApplicationData)
			if outerType := c.config.Bugs.OuterRecordType; outerType != 0 {
				record[0] = byte(outerType)
			}
		}
		record[1] = byte(vers >> 8)
		record[2] = byte(vers)
		record[3] = byte(m >> 8)
		record[4] = byte(m)

		record, err = c.out.encrypt(epoch, record, data[:m], typ, tlsRecordHeaderLen, true                        )
		if err != nil {
			return
		}
		_, err = c.conn.Write(record)
		if err != nil {
			break
		}
		n += m
		data = data[m:]
	}

	if typ == recordTypeChangeCipherSpec && c.vers < VersionTLS13 {
		err = c.out.changeCipherSpec()
		if err != nil {
			return n, c.sendAlertLocked(alertLevelError, err.(alert))
		}
	}
	return
}

func (c *Conn) flushHandshake() error {
	if c.isDTLS {
		return c.dtlsFlushHandshake()
	}

	for c.pendingFlight.Len() > 0 {
		var buf [maxPlaintext]byte
		n, _ := c.pendingFlight.Read(buf[:])
		if _, err := c.doWriteRecord(recordTypeHandshake, buf[:n]); err != nil {
			return err
		}
	}

	c.pendingFlight.Reset()
	return nil
}

func (c *Conn) ackHandshake() error {
	if c.isDTLS {
		return c.dtlsACKHandshake()
	}
	return nil
}

func (c *Conn) doReadHandshake() ([]byte, error) {
	if c.isDTLS {
		return c.dtlsDoReadHandshake()
	}

	for c.hand.Len() < 4 {
		if err := c.in.err; err != nil {
			return nil, err
		}
		if err := c.readRecord(recordTypeHandshake); err != nil {
			return nil, err
		}
	}

	data := c.hand.Bytes()
	n := int(data[1])<<16 | int(data[2])<<8 | int(data[3])
	if n > maxHandshake {
		return nil, c.in.setErrorLocked(c.sendAlert(alertInternalError))
	}
	for c.hand.Len() < 4+n {
		if err := c.in.err; err != nil {
			return nil, err
		}
		if err := c.readRecord(recordTypeHandshake); err != nil {
			return nil, err
		}
	}
	return c.hand.Next(4 + n), nil
}

func (c *Conn) readHandshake() (any, error) {
	data, err := c.doReadHandshake()
	if err != nil {
		return nil, err
	}

	typ := data[0]
	var m handshakeMessage
	switch typ {
	case typeHelloRequest:
		m = new(helloRequestMsg)
	case typeClientHello:
		m = &clientHelloMsg{
			isDTLS: c.isDTLS,
		}
	case typeServerHello:
		m = &serverHelloMsg{
			isDTLS: c.isDTLS,
		}
	case typeNewSessionTicket:
		m = &newSessionTicketMsg{
			vers:   c.wireVersion,
			isDTLS: c.isDTLS,
		}
	case typeEncryptedExtensions:
		if c.isClient {
			m = new(encryptedExtensionsMsg)
		} else {
			m = new(clientEncryptedExtensionsMsg)
		}
	case typeCertificate:
		m = &certificateMsg{
			hasRequestContext: c.vers >= VersionTLS13,
		}
	case typeCompressedCertificate:
		m = new(compressedCertificateMsg)
	case typeCertificateRequest:
		m = &certificateRequestMsg{
			vers:                  c.wireVersion,
			hasSignatureAlgorithm: c.vers >= VersionTLS12,
			hasRequestContext:     c.vers >= VersionTLS13,
		}
	case typeCertificateStatus:
		m = new(certificateStatusMsg)
	case typeServerKeyExchange:
		m = new(serverKeyExchangeMsg)
	case typeServerHelloDone:
		m = new(serverHelloDoneMsg)
	case typeClientKeyExchange:
		m = new(clientKeyExchangeMsg)
	case typeCertificateVerify:
		m = &certificateVerifyMsg{
			hasSignatureAlgorithm: c.vers >= VersionTLS12,
		}
	case typeNextProtocol:
		m = new(nextProtoMsg)
	case typeFinished:
		m = new(finishedMsg)
	case typeHelloVerifyRequest:
		m = new(helloVerifyRequestMsg)
	case typeChannelID:
		m = new(channelIDMsg)
	case typeKeyUpdate:
		m = new(keyUpdateMsg)
	case typeEndOfEarlyData:
		m = new(endOfEarlyDataMsg)
	default:
		return nil, c.in.setErrorLocked(c.sendAlert(alertUnexpectedMessage))
	}

	data = slices.Clone(data)

	if data[0] == typeServerHello && len(data) >= 38 {
		vers := uint16(data[4])<<8 | uint16(data[5])
		if (vers == VersionDTLS12 || vers == VersionTLS12) && bytes.Equal(data[6:38], tls13HelloRetryRequest) {
			m = &helloRetryRequestMsg{isDTLS: c.isDTLS}
		}
	}

	if !m.unmarshal(data) {
		c.sendAlert(alertDecodeError)
		return nil, c.in.setErrorLocked(fmt.Errorf("tls: error decoding %s message", messageTypeToString(typ)))
	}
	return m, nil
}

func readHandshakeType[T any](c *Conn) (*T, error) {
	m, err := c.readHandshake()
	if err != nil {
		return nil, err
	}
	mType, ok := m.(*T)
	if !ok {
		c.sendAlert(alertUnexpectedMessage)
		return nil, unexpectedMessageError(mType, m)
	}
	return mType, nil
}

func (c *Conn) SendHalfHelloRequest() error {
	if err := c.Handshake(); err != nil {
		return err
	}

	c.out.Lock()
	defer c.out.Unlock()

	if _, err := c.writeRecord(recordTypeHandshake, []byte{typeHelloRequest, 0}); err != nil {
		return err
	}
	return c.flushHandshake()
}

func (c *Conn) Write(b []byte) (int, error) {
	if err := c.Handshake(); err != nil {
		return 0, err
	}

	c.out.Lock()
	defer c.out.Unlock()

	if err := c.out.err; err != nil {
		return 0, err
	}

	if !c.handshakeComplete {
		return 0, alertInternalError
	}

	if c.keyUpdateRequested {
		if err := c.sendKeyUpdateLocked(keyUpdateNotRequested); err != nil {
			return 0, err
		}
		c.keyUpdateRequested = false
	}

	if c.config.Bugs.SendSpuriousAlert != 0 {
		c.sendAlertLocked(alertLevelError, c.config.Bugs.SendSpuriousAlert)
	}

	if c.config.Bugs.SendHelloRequestBeforeEveryAppDataRecord {
		c.writeRecord(recordTypeHandshake, []byte{typeHelloRequest, 0, 0, 0})
		c.flushHandshake()
	}

	var m int
	if len(b) > 1 && c.vers <= VersionTLS10 && !c.isDTLS {
		if _, ok := c.out.epoch.cipher.(*cbcMode); ok {
			n, err := c.writeRecord(recordTypeApplicationData, b[:1])
			if err != nil {
				return n, c.out.setErrorLocked(err)
			}
			m, b = 1, b[1:]
		}
	}

	n, err := c.writeRecord(recordTypeApplicationData, b)
	return n + m, c.out.setErrorLocked(err)
}

func (c *Conn) processTLS13NewSessionTicket(newSessionTicket *newSessionTicketMsg, cipherSuite *cipherSuite) error {
	session := &ClientSessionState{
		sessionTicket:               newSessionTicket.ticket,
		vers:                        c.vers,
		wireVersion:                 c.wireVersion,
		cipherSuite:                 cipherSuite,
		secret:                      deriveSessionPSK(cipherSuite, c.wireVersion, c.resumptionSecret, newSessionTicket.ticketNonce, c.isDTLS),
		serverCertificates:          c.peerCertificates,
		sctList:                     c.sctList,
		ocspResponse:                c.ocspResponse,
		ticketCreationTime:          c.config.time(),
		ticketExpiration:            c.config.time().Add(time.Duration(newSessionTicket.ticketLifetime) * time.Second),
		ticketAgeAdd:                newSessionTicket.ticketAgeAdd,
		maxEarlyDataSize:            newSessionTicket.maxEarlyDataSize,
		earlyALPN:                   c.clientProtocol,
		hasApplicationSettings:      c.hasApplicationSettings,
		localApplicationSettings:    c.localApplicationSettings,
		peerApplicationSettings:     c.peerApplicationSettings,
		hasApplicationSettingsOld:   c.hasApplicationSettingsOld,
		localApplicationSettingsOld: c.localApplicationSettingsOld,
		peerApplicationSettingsOld:  c.peerApplicationSettingsOld,
		resumptionAcrossNames:       newSessionTicket.flags.hasFlag(flagResumptionAcrossNames),
	}

	if c.config.Bugs.ExpectGREASE && !newSessionTicket.hasGREASEExtension {
		return errors.New("tls: no GREASE ticket extension found")
	}

	if c.config.Bugs.ExpectTicketEarlyData && newSessionTicket.maxEarlyDataSize == 0 {
		return errors.New("tls: no early_data ticket extension found")
	}

	if c.config.Bugs.ExpectNoNewSessionTicket || c.config.Bugs.ExpectNoNonEmptyNewSessionTicket {
		return errors.New("tls: received unexpected NewSessionTicket")
	}

	if expect := c.config.Bugs.ExpectResumptionAcrossNames; expect != nil && session.resumptionAcrossNames != *expect {
		return errors.New("tls: resumption_across_names status of ticket did not match expectation")
	}

	if c.config.ClientSessionCache == nil || newSessionTicket.ticketLifetime == 0 {
		return nil
	}

	cacheKey := clientSessionCacheKey(c.conn.RemoteAddr(), c.config)
	_, ok := c.config.ClientSessionCache.Get(cacheKey)
	if !ok || !c.config.Bugs.UseFirstSessionTicket {
		c.config.ClientSessionCache.Put(cacheKey, session)
	}

	return c.ackHandshake()
}

func (c *Conn) processKeyUpdate(keyUpdate *keyUpdateMsg) error {
	epoch := c.in.epoch.epoch + 1
	if epoch == 0 && !c.config.Bugs.AllowEpochOverflow {
		return errors.New("tls: too many KeyUpdates")
	}
	if err := c.useInTrafficSecret(epoch, c.in.wireVersion, c.cipherSuite, updateTrafficSecret(c.cipherSuite.hash(), c.wireVersion, c.in.trafficSecret, c.isDTLS)); err != nil {
		return err
	}
	if keyUpdate.keyUpdateRequest == keyUpdateRequested {
		c.keyUpdateRequested = true
	}
	return c.ackHandshake()
}

func (c *Conn) handlePostHandshakeMessage() error {
	msg, err := c.readHandshake()
	if err != nil {
		return err
	}

	if c.vers < VersionTLS13 {
		if !c.isClient {
			c.sendAlert(alertUnexpectedMessage)
			return errors.New("tls: unexpected post-handshake message")
		}

		_, ok := msg.(*helloRequestMsg)
		if !ok {
			c.sendAlert(alertUnexpectedMessage)
			return alertUnexpectedMessage
		}

		c.handshakeComplete = false
		return c.Handshake()
	}

	if c.isClient {
		if newSessionTicket, ok := msg.(*newSessionTicketMsg); ok {
			return c.processTLS13NewSessionTicket(newSessionTicket, c.cipherSuite)
		}
	}

	if keyUpdate, ok := msg.(*keyUpdateMsg); ok {
		c.keyUpdateSeen = true
		if c.config.Bugs.RejectUnsolicitedKeyUpdate {
			return errors.New("tls: unexpected KeyUpdate message")
		}
		return c.processKeyUpdate(keyUpdate)
	}

	c.sendAlert(alertUnexpectedMessage)
	return errors.New("tls: unexpected post-handshake message")
}

func (c *Conn) ReadKeyUpdate() error {
	c.in.Lock()
	defer c.in.Unlock()

	keyUpdate, err := readHandshakeType[keyUpdateMsg](c)
	if err != nil {
		return err
	}

	if keyUpdate.keyUpdateRequest != keyUpdateNotRequested {
		return errors.New("tls: received invalid KeyUpdate message")
	}

	return c.processKeyUpdate(keyUpdate)
}

func (c *Conn) Renegotiate() error {
	if !c.isClient {
		helloReq := new(helloRequestMsg).marshal()
		if c.config.Bugs.BadHelloRequest != nil {
			helloReq = c.config.Bugs.BadHelloRequest
		}
		c.writeRecord(recordTypeHandshake, helloReq)
		c.flushHandshake()
	}

	c.handshakeComplete = false
	return c.Handshake()
}

func (c *Conn) Read(b []byte) (n int, err error) {
	if err = c.Handshake(); err != nil {
		return
	}

	c.in.Lock()
	defer c.in.Unlock()

	const maxConsecutiveEmptyRecords = 100
	for emptyRecordCount := 0; emptyRecordCount <= maxConsecutiveEmptyRecords; emptyRecordCount++ {
		for c.input.Len() == 0 && c.in.err == nil {
			if err := c.readRecord(recordTypeApplicationData); err != nil {

				return 0, err
			}
			for c.hand.Len() > 0 {

				if err := c.handlePostHandshakeMessage(); err != nil {
					return 0, err
				}
			}
		}
		if err := c.in.err; err != nil {
			return 0, err
		}

		n, err = c.input.Read(b)
		if c.input.Len() == 0 || c.isDTLS {
			c.input.Reset()
		}

		if ri := c.rawInput.Bytes(); !c.isDTLS && n != 0 && err == nil &&
			c.input.Len() == 0 && len(ri) > 0 && recordType(ri[0]) == recordTypeAlert {
			if recErr := c.readRecord(recordTypeApplicationData); recErr != nil {
				err = recErr
			}
		}

		if n != 0 || err != nil {
			return n, err
		}
	}

	return 0, io.ErrNoProgress
}

func (c *Conn) Close() error {
	var alertErr error

	c.handshakeMutex.Lock()
	defer c.handshakeMutex.Unlock()
	if c.handshakeComplete && !c.config.Bugs.NoCloseNotify {
		alert := alertCloseNotify
		if c.config.Bugs.SendAlertOnShutdown != 0 {
			alert = c.config.Bugs.SendAlertOnShutdown
		}
		alertErr = c.sendAlert(alert)

		if opErr, ok := alertErr.(*net.OpError); ok && opErr.Op == "local error" {
			alertErr = nil
		}
	}

	if c.handshakeComplete && alertErr == nil && c.config.Bugs.ExpectCloseNotify {
		for c.in.error() == nil {
			c.readRecord(recordTypeAlert)
		}
		if c.in.error() != io.EOF {
			alertErr = c.in.error()
		}
	}

	if err := c.conn.Close(); err != nil {
		return err
	}
	return alertErr
}

func (c *Conn) Handshake() error {
	c.handshakeMutex.Lock()
	defer c.handshakeMutex.Unlock()
	if err := c.handshakeErr; err != nil {
		return err
	}
	if c.handshakeComplete {
		return nil
	}

	if c.isDTLS && c.config.Bugs.SendSplitAlert {
		c.conn.Write([]byte{
			byte(recordTypeAlert),
			0xfe, 0xff,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x2,
		})
		c.conn.Write([]byte{alertLevelError, byte(alertInternalError)})
	}
	if data := c.config.Bugs.AppDataBeforeHandshake; data != nil {
		c.writeRecord(recordTypeApplicationData, data)
	}
	if c.isClient {
		c.handshakeErr = c.clientHandshake()
	} else {
		c.handshakeErr = c.serverHandshake()
	}
	if c.handshakeErr == nil && c.config.Bugs.SendInvalidRecordType {
		c.writeRecord(recordType(42), []byte("invalid record"))
	}
	return c.handshakeErr
}

func (c *Conn) ConnectionState() ConnectionState {
	c.handshakeMutex.Lock()
	defer c.handshakeMutex.Unlock()

	var state ConnectionState
	state.HandshakeComplete = c.handshakeComplete
	if c.handshakeComplete {
		state.Version = c.vers
		state.NegotiatedProtocol = c.clientProtocol
		state.DidResume = c.didResume
		state.NegotiatedProtocolIsMutual = !c.clientProtocolFallback
		state.NegotiatedProtocolFromALPN = c.usedALPN
		state.CipherSuite = c.cipherSuite.id
		state.PeerCertificates = c.peerCertificates
		state.PeerDelegatedCredential = c.peerDelegatedCredential
		state.VerifiedChains = c.verifiedChains
		state.OCSPResponse = c.ocspResponse
		state.ServerName = c.serverName
		state.ChannelID = c.channelID
		state.SRTPProtectionProfile = c.srtpProtectionProfile
		state.TLSUnique = c.firstFinished[:]
		state.SCTList = c.sctList
		state.PeerSignatureAlgorithm = c.peerSignatureAlgorithm
		state.CurveID = c.curveID
		state.QUICTransportParams = c.quicTransportParams
		state.QUICTransportParamsLegacy = c.quicTransportParamsLegacy
		state.HasApplicationSettings = c.hasApplicationSettings
		state.PeerApplicationSettings = c.peerApplicationSettings
		state.HasApplicationSettingsOld = c.hasApplicationSettingsOld
		state.PeerApplicationSettingsOld = c.peerApplicationSettingsOld
		state.ECHAccepted = c.echAccepted
	}

	return state
}

func (c *Conn) VerifyHostname(host string) error {
	c.handshakeMutex.Lock()
	defer c.handshakeMutex.Unlock()
	if !c.isClient {
		return errors.New("tls: VerifyHostname called on TLS server connection")
	}
	if !c.handshakeComplete {
		return errors.New("tls: handshake has not yet been performed")
	}
	return c.peerCertificates[0].VerifyHostname(host)
}

func (c *Conn) exportKeyingMaterialTLS13(length int, secret, label, context []byte) []byte {
	hash := c.cipherSuite.hash()
	exporterKeyingLabel := []byte("exporter")
	contextHash := hash.New()
	contextHash.Write(context)
	exporterContext := hash.New().Sum(nil)
	derivedSecret := hkdfExpandLabel(c.cipherSuite.hash(), secret, label, exporterContext, hash.Size(), c.isDTLS)
	return hkdfExpandLabel(c.cipherSuite.hash(), derivedSecret, exporterKeyingLabel, contextHash.Sum(nil), length, c.isDTLS)
}

func (c *Conn) ExportKeyingMaterial(length int, label, context []byte, useContext bool) ([]byte, error) {
	c.handshakeMutex.Lock()
	defer c.handshakeMutex.Unlock()
	if !c.handshakeComplete {
		return nil, errors.New("tls: handshake has not yet been performed")
	}

	if c.vers >= VersionTLS13 {
		return c.exportKeyingMaterialTLS13(length, c.exporterSecret, label, context), nil
	}

	seedLen := len(c.clientRandom) + len(c.serverRandom)
	if useContext {
		seedLen += 2 + len(context)
	}
	seed := make([]byte, 0, seedLen)
	seed = append(seed, c.clientRandom[:]...)
	seed = append(seed, c.serverRandom[:]...)
	if useContext {
		seed = append(seed, byte(len(context)>>8), byte(len(context)))
		seed = append(seed, context...)
	}
	result := make([]byte, length)
	prfForVersion(c.vers, c.cipherSuite)(result, c.exporterSecret, label, seed)
	return result, nil
}

func (c *Conn) ExportEarlyKeyingMaterial(length int, label, context []byte) ([]byte, error) {
	if c.vers < VersionTLS13 {
		return nil, errors.New("tls: early exporters not defined before TLS 1.3")
	}

	if c.earlyExporterSecret == nil {
		return nil, errors.New("tls: no early exporter secret")
	}

	return c.exportKeyingMaterialTLS13(length, c.earlyExporterSecret, label, context), nil
}

func (c *Conn) noRenegotiationInfo() bool {
	if c.config.Bugs.NoRenegotiationInfo {
		return true
	}
	if c.cipherSuite == nil && c.config.Bugs.NoRenegotiationInfoInInitial {
		return true
	}
	if c.cipherSuite != nil && c.config.Bugs.NoRenegotiationInfoAfterInitial {
		return true
	}
	return false
}

func (c *Conn) SendNewSessionTicket(nonce []byte) error {
	if c.isClient || c.vers < VersionTLS13 {
		return errors.New("tls: cannot send post-handshake NewSessionTicket")
	}

	var peerCertificatesRaw [][]byte
	for _, cert := range c.peerCertificates {
		peerCertificatesRaw = append(peerCertificatesRaw, cert.Raw)
	}

	addBuffer := make([]byte, 4)
	_, err := io.ReadFull(c.config.rand(), addBuffer)
	if err != nil {
		c.sendAlert(alertInternalError)
		return errors.New("tls: short read from Rand: " + err.Error())
	}
	ticketAgeAdd := uint32(addBuffer[3])<<24 | uint32(addBuffer[2])<<16 | uint32(addBuffer[1])<<8 | uint32(addBuffer[0])

	m := &newSessionTicketMsg{
		vers:                        c.wireVersion,
		isDTLS:                      c.isDTLS,
		ticketLifetime:              uint32(24 * time.Hour / time.Second),
		duplicateEarlyDataExtension: c.config.Bugs.DuplicateTicketEarlyData,
		customExtension:             c.config.Bugs.CustomTicketExtension,
		ticketAgeAdd:                ticketAgeAdd,
		ticketNonce:                 nonce,
		maxEarlyDataSize:            c.config.MaxEarlyDataSize,
		flags: flagSet{
			mustInclude: c.config.Bugs.AlwaysSendTicketFlags,
			padding:     c.config.Bugs.TicketFlagPadding,
		},
	}
	if c.config.Bugs.MockQUICTransport != nil && m.maxEarlyDataSize > 0 {
		m.maxEarlyDataSize = 0xffffffff
	}

	if c.config.Bugs.SendTicketLifetime != 0 {
		m.ticketLifetime = uint32(c.config.Bugs.SendTicketLifetime / time.Second)
	}
	if c.config.ResumptionAcrossNames {
		m.flags.setFlag(flagResumptionAcrossNames)
	}
	for _, flag := range c.config.Bugs.SendTicketFlags {
		m.flags.setFlag(flag)
	}

	state := sessionState{
		vers:                        c.vers,
		cipherSuite:                 c.cipherSuite.id,
		secret:                      deriveSessionPSK(c.cipherSuite, c.wireVersion, c.resumptionSecret, nonce, c.isDTLS),
		certificates:                peerCertificatesRaw,
		ticketCreationTime:          c.config.time(),
		ticketExpiration:            c.config.time().Add(time.Duration(m.ticketLifetime) * time.Second),
		ticketAgeAdd:                uint32(addBuffer[3])<<24 | uint32(addBuffer[2])<<16 | uint32(addBuffer[1])<<8 | uint32(addBuffer[0]),
		earlyALPN:                   []byte(c.clientProtocol),
		hasApplicationSettings:      c.hasApplicationSettings,
		localApplicationSettings:    c.localApplicationSettings,
		peerApplicationSettings:     c.peerApplicationSettings,
		hasApplicationSettingsOld:   c.hasApplicationSettingsOld,
		localApplicationSettingsOld: c.localApplicationSettingsOld,
		peerApplicationSettingsOld:  c.peerApplicationSettingsOld,
	}

	if !c.config.Bugs.SendEmptySessionTicket {
		var err error
		m.ticket, err = c.encryptTicket(&state)
		if err != nil {
			return err
		}
	}
	c.out.Lock()
	defer c.out.Unlock()
	_, err = c.writeRecord(recordTypeHandshake, m.marshal())
	return err
}

func (c *Conn) SendKeyUpdate(keyUpdateRequest byte) error {
	c.out.Lock()
	defer c.out.Unlock()
	return c.sendKeyUpdateLocked(keyUpdateRequest)
}

func (c *Conn) sendKeyUpdateLocked(keyUpdateRequest byte) error {
	if c.vers < VersionTLS13 {
		return errors.New("tls: attempted to send KeyUpdate before TLS 1.3")
	}
	epoch := c.out.epoch.epoch + 1
	if epoch == 0 && !c.config.Bugs.AllowEpochOverflow {
		return errors.New("tls: too many KeyUpdates")
	}

	m := keyUpdateMsg{
		keyUpdateRequest: keyUpdateRequest,
	}
	if _, err := c.writeRecord(recordTypeHandshake, m.marshal()); err != nil {
		return err
	}

	c.useOutTrafficSecret(epoch, c.out.wireVersion, c.cipherSuite, updateTrafficSecret(c.cipherSuite.hash(), c.wireVersion, c.out.trafficSecret, c.isDTLS))
	return c.flushHandshake()
}

func (c *Conn) sendFakeEarlyData(len int) error {

	payload := make([]byte, 5+len)
	payload[0] = byte(recordTypeApplicationData)
	payload[1] = 3
	payload[2] = 3
	payload[3] = byte(len >> 8)
	payload[4] = byte(len)
	_, err := c.conn.Write(payload)
	return err
}

func (c *Conn) usesEndOfEarlyData() bool {
	if c.isClient && c.config.Bugs.SendEndOfEarlyDataInQUICAndDTLS {
		return true
	}
	return c.config.Bugs.MockQUICTransport == nil && !c.isDTLS
}
