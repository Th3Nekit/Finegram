// Copyright 2014 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// DTLS implementation.
//
// NOTE: This is a not even a remotely production-quality DTLS
// implementation. It is the bare minimum necessary to be able to
// achieve coverage on BoringSSL's implementation. Of note is that
// this implementation assumes the underlying net.PacketConn is not
// only reliable but also ordered. BoringSSL will be expected to deal
// with simulated loss, but there is no point in forcing the test
// driver to.

package runner

import (
	"bytes"
	"cmp"
	"encoding/binary"
	"errors"
	"fmt"
	"math/rand"
	"net"
	"slices"
	"time"

	"golang.org/x/crypto/cryptobyte"
)

type DTLSMessage struct {
	Epoch              uint16
	IsChangeCipherSpec bool

	Type     uint8
	Sequence uint16
	Data     []byte
}

func (m *DTLSMessage) Fragment(offset, length int) DTLSFragment {
	if m.IsChangeCipherSpec {

		return DTLSFragment{
			Epoch:              m.Epoch,
			IsChangeCipherSpec: m.IsChangeCipherSpec,
			Data:               m.Data,
		}
	}

	return DTLSFragment{
		Epoch:       m.Epoch,
		Sequence:    m.Sequence,
		Type:        m.Type,
		Data:        m.Data[offset : offset+length],
		Offset:      offset,
		TotalLength: len(m.Data),
	}
}

func (m *DTLSMessage) Split(offset int) (DTLSFragment, DTLSFragment) {
	if m.IsChangeCipherSpec {
		panic("tls: cannot split ChangeCipherSpec")
	}

	return m.Fragment(0, offset), m.Fragment(offset, len(m.Data)-offset)
}

type DTLSFragment struct {
	Epoch              uint16
	IsChangeCipherSpec bool

	Type        uint8
	TotalLength int
	Sequence    uint16
	Offset      int
	Data        []byte

	ShouldDiscard bool
}

func (f *DTLSFragment) Bytes() []byte {
	if f.IsChangeCipherSpec {
		return f.Data
	}

	bb := cryptobyte.NewBuilder(make([]byte, 0, 12+len(f.Data)))
	bb.AddUint8(f.Type)
	bb.AddUint24(uint32(f.TotalLength))
	bb.AddUint16(f.Sequence)
	bb.AddUint24(uint32(f.Offset))
	addUint24LengthPrefixedBytes(bb, f.Data)
	return bb.BytesOrPanic()
}

func comparePair[T1 cmp.Ordered, T2 cmp.Ordered](a1 T1, a2 T2, b1 T1, b2 T2) int {
	cmp1 := cmp.Compare(a1, b1)
	if cmp1 != 0 {
		return cmp1
	}
	return cmp.Compare(a2, b2)
}

type DTLSRecordNumber struct {

	Epoch    uint64
	Sequence uint64
}

type DTLSRecordNumberInfo struct {
	DTLSRecordNumber

	MessageStartSequence uint16
	MessageStartOffset   int

	MessageEndSequence uint16
	MessageEndOffset   int
}

func (r *DTLSRecordNumberInfo) HasACKInformation() bool {
	return comparePair(r.MessageStartSequence, r.MessageStartOffset, r.MessageEndSequence, r.MessageEndOffset) < 0
}

func (c *Conn) readDTLS13RecordHeader(epoch *epochState, b []byte) (headerLen int, recordLen int, recTyp recordType, err error) {

	recordHeaderLen := 5
	if len(b) < recordHeaderLen {
		return 0, 0, 0, errors.New("dtls: failed to read record header")
	}
	typ := b[0]
	if typ&0xfc != 0x2c {
		return 0, 0, 0, errors.New("dtls: DTLS 1.3 record header has bad type byte")
	}

	epochBits := typ & 0x03
	if epochBits != byte(epoch.epoch&0x03) {
		c.sendAlert(alertIllegalParameter)
		return 0, 0, 0, c.in.setErrorLocked(fmt.Errorf("dtls: bad epoch"))
	}
	wireSeq := b[1:3]
	if !c.config.Bugs.NullAllCiphers {
		sample := b[recordHeaderLen:]
		mask := epoch.recordNumberEncrypter.generateMask(sample)
		xorSlice(wireSeq, mask)
	}
	decWireSeq := binary.BigEndian.Uint16(wireSeq)

	seqInt := binary.BigEndian.Uint64(epoch.seq[:])

	seqInt = seqInt &^ (0xffff << 48)
	newSeq := seqInt&^0xffff | uint64(decWireSeq)
	if newSeq < seqInt {
		newSeq += 0x10000
	}

	seq := make([]byte, 8)
	binary.BigEndian.PutUint64(seq, newSeq)
	copy(epoch.seq[2:], seq[2:])

	recordLen = int(b[3])<<8 | int(b[4])
	return recordHeaderLen, recordLen, 0, nil
}

func (c *Conn) readDTLSRecordHeader(epoch *epochState, b []byte) (headerLen int, recordLen int, typ recordType, err error) {
	if epoch.cipher != nil && c.in.version >= VersionTLS13 {
		return c.readDTLS13RecordHeader(epoch, b)
	}

	recordHeaderLen := 13

	if len(b) < recordHeaderLen {
		return 0, 0, 0, errors.New("dtls: failed to read record header")
	}
	typ = recordType(b[0])
	vers := uint16(b[1])<<8 | uint16(b[2])

	if typ != recordTypeAlert && !c.skipRecordVersionCheck {
		if c.haveVers {
			wireVersion := c.wireVersion
			if c.vers >= VersionTLS13 {
				wireVersion = VersionDTLS12
			}
			if vers != wireVersion {
				c.sendAlert(alertProtocolVersion)
				return 0, 0, 0, c.in.setErrorLocked(fmt.Errorf("dtls: received record with version %x when expecting version %x", vers, c.wireVersion))
			}
		} else {
			if expect := c.config.Bugs.ExpectInitialRecordVersion; expect != 0 && vers != expect {
				c.sendAlert(alertProtocolVersion)
				return 0, 0, 0, c.in.setErrorLocked(fmt.Errorf("dtls: received record with version %x when expecting version %x", vers, expect))
			}
		}
	}
	epochValue := binary.BigEndian.Uint16(b[3:5])
	seq := b[5:11]

	if epochValue != epoch.epoch {
		c.sendAlert(alertIllegalParameter)
		return 0, 0, 0, c.in.setErrorLocked(fmt.Errorf("dtls: bad epoch"))
	}
	if bytes.Compare(seq, epoch.seq[2:]) < 0 {
		c.sendAlert(alertIllegalParameter)
		return 0, 0, 0, c.in.setErrorLocked(fmt.Errorf("dtls: bad sequence number"))
	}
	copy(epoch.seq[2:], seq)
	recordLen = int(b[11])<<8 | int(b[12])
	return recordHeaderLen, recordLen, typ, nil
}

func (c *Conn) dtlsDoReadRecord(epoch *epochState, want recordType) (recordType, []byte, error) {

	var newPacket bool
	bytesAvailableInLastPacket := c.bytesAvailableInPacket
	if c.rawInput.Len() == 0 {

		c.rawInput.Grow(maxCiphertext + dtlsMaxRecordHeaderLen)
		buf := c.rawInput.AvailableBuffer()
		n, err := c.conn.Read(buf[:cap(buf)])
		if err != nil {
			return 0, nil, err
		}
		if c.maxPacketLen != 0 {
			if n > c.maxPacketLen {
				return 0, nil, fmt.Errorf("dtls: exceeded maximum packet length")
			}
			c.bytesAvailableInPacket = c.maxPacketLen - n
		} else {
			c.bytesAvailableInPacket = 0
		}
		c.rawInput.Write(buf[:n])
		newPacket = true
	}

	recordHeaderLen, n, typ, err := c.readDTLSRecordHeader(epoch, c.rawInput.Bytes())
	if err != nil {
		return 0, nil, err
	}
	if n > maxCiphertext || c.rawInput.Len() < recordHeaderLen+n {
		c.sendAlert(alertRecordOverflow)
		return 0, nil, c.in.setErrorLocked(fmt.Errorf("dtls: oversized record received with length %d", n))
	}
	b := c.rawInput.Next(recordHeaderLen + n)

	ok, encTyp, data, alertValue := c.in.decrypt(epoch, recordHeaderLen, b)
	if !ok {

		return 0, nil, c.in.setErrorLocked(c.sendAlert(alertValue))
	}

	if typ == 0 {

		typ = encTyp
	}

	if typ == recordTypeChangeCipherSpec || typ == recordTypeHandshake {

		if c.lastRecordInFlight != nil {

			const handshakeBytesNeeded = 13
			if typ == recordTypeHandshake && c.lastRecordInFlight.typ == recordTypeHandshake && epoch.epoch == c.lastRecordInFlight.epoch {

				if c.lastRecordInFlight.bytesAvailable > handshakeBytesNeeded && epoch.epoch > 0 {
					return 0, nil, c.in.setErrorLocked(fmt.Errorf("dtls: previous handshake record had %d bytes available, but shim did not fit another fragment in it", c.lastRecordInFlight.bytesAvailable))
				}
			} else if newPacket {

				bytesNeeded := 1
				if typ == recordTypeHandshake {
					bytesNeeded = handshakeBytesNeeded
				}
				bytesNeeded += recordHeaderLen + c.in.maxEncryptOverhead(epoch, bytesNeeded)
				if bytesNeeded < bytesAvailableInLastPacket {
					return 0, nil, c.in.setErrorLocked(fmt.Errorf("dtls: previous packet had %d bytes available, but shim did not fit record of type %d into it", bytesAvailableInLastPacket, typ))
				}
			}
		}

		recordBytesAvailable := c.bytesAvailableInPacket + c.rawInput.Len()
		if cbc, ok := epoch.cipher.(*cbcMode); ok {

			recordBytesAvailable = max(0, recordBytesAvailable-cbc.BlockSize())
		}
		c.lastRecordInFlight = &dtlsRecordInfo{typ: typ, epoch: epoch.epoch, bytesAvailable: recordBytesAvailable}
	} else {
		c.lastRecordInFlight = nil
	}

	return typ, data, nil
}

func (c *Conn) dtlsWriteRecord(typ recordType, data []byte) (n int, err error) {
	epoch := &c.out.epoch

	if typ == recordTypeChangeCipherSpec {

		if c.vers >= VersionTLS13 {
			return
		}
		c.nextFlight = append(c.nextFlight, DTLSMessage{
			Epoch:              epoch.epoch,
			IsChangeCipherSpec: true,
			Data:               slices.Clone(data),
		})
		err = c.out.changeCipherSpec()
		if err != nil {
			return n, c.sendAlertLocked(alertLevelError, err.(alert))
		}
		return len(data), nil
	}
	if typ == recordTypeHandshake {

		header := data[:4]
		body := data[4:]

		c.nextFlight = append(c.nextFlight, DTLSMessage{
			Epoch:    epoch.epoch,
			Sequence: c.sendHandshakeSeq,
			Type:     header[0],
			Data:     slices.Clone(body),
		})
		c.sendHandshakeSeq++
		return len(data), nil
	}

	err = c.dtlsWriteFlight()
	if err != nil {
		return
	}

	if typ == recordTypeApplicationData && len(data) > 1 && c.config.Bugs.SplitAndPackAppData {
		_, _, err = c.dtlsPackRecord(epoch, typ, data[:len(data)/2], false)
		if err != nil {
			return
		}
		_, _, err = c.dtlsPackRecord(epoch, typ, data[len(data)/2:], true)
		if err != nil {
			return
		}
		n = len(data)
	} else {
		n, _, err = c.dtlsPackRecord(epoch, typ, data, false)
		if err != nil {
			return
		}
	}

	err = c.dtlsFlushPacket()
	return
}

func (c *Conn) dtlsWriteFlight() error {
	if len(c.nextFlight) == 0 {
		return nil
	}

	prev, received, next, records := c.previousFlight, c.receivedFlight, c.nextFlight, c.receivedFlightRecords
	c.previousFlight, c.receivedFlight, c.nextFlight, c.receivedFlightRecords = next, nil, nil, nil

	controller := newDTLSController(c, received)
	if c.config.Bugs.WriteFlightDTLS != nil {
		c.config.Bugs.WriteFlightDTLS(&controller, prev, received, next, records)
	} else {
		controller.WriteFlight(next)
	}
	if err := controller.Err(); err != nil {
		return err
	}

	if c.receivedFlight != nil || c.receivedFlightRecords != nil || c.nextFlight != nil {
		panic("tls: flight state changed while writing flight")
	}
	if controller.mergeIntoNextFlight {
		c.previousFlight, c.receivedFlight, c.nextFlight, c.receivedFlightRecords = prev, received, next, records
	}

	return c.dtlsFlushPacket()
}

func (c *Conn) dtlsFlushHandshake() error {
	if err := c.dtlsWriteFlight(); err != nil {
		return err
	}
	if err := c.dtlsFlushPacket(); err != nil {
		return err
	}

	return nil
}

func (c *Conn) dtlsACKHandshake() error {
	if len(c.receivedFlight) == 0 {
		return nil
	}

	if len(c.nextFlight) != 0 {
		panic("tls: not a final flight; more messages were queued up")
	}

	prev, received, records := c.previousFlight, c.receivedFlight, c.receivedFlightRecords
	c.previousFlight, c.receivedFlight, c.receivedFlightRecords = nil, nil, nil

	controller := newDTLSController(c, received)
	if c.config.Bugs.ACKFlightDTLS != nil {
		c.config.Bugs.ACKFlightDTLS(&controller, prev, received, records)
	} else {
		if c.vers >= VersionTLS13 {
			controller.WriteACK(controller.OutEpoch(), records)
		}
	}
	if err := controller.Err(); err != nil {
		return err
	}

	if c.previousFlight != nil || c.receivedFlight != nil || c.receivedFlightRecords != nil {
		panic("tls: flight state changed while ACKing flight")
	}
	if controller.mergeIntoNextFlight {
		c.previousFlight, c.receivedFlight, c.receivedFlightRecords = prev, received, records
	}

	return c.dtlsFlushPacket()
}

func (c *Conn) appendDTLS13RecordHeader(b, seq []byte, recordLen int) []byte {

	typ := byte(0x20)

	if c.config.Bugs.DTLS13RecordHeaderSetCIDBit && c.handshakeComplete {
		typ |= 0x10
	}

	if !c.config.DTLSUseShortSeqNums {
		typ |= 0x08
	}

	if !c.config.DTLSRecordHeaderOmitLength {
		typ |= 0x04
	}

	typ |= seq[1] & 0x3
	b = append(b, typ)
	if c.config.DTLSUseShortSeqNums {
		b = append(b, seq[7])
	} else {
		b = append(b, seq[6], seq[7])
	}
	if !c.config.DTLSRecordHeaderOmitLength {
		b = append(b, byte(recordLen>>8), byte(recordLen))
	}
	return b
}

func (c *Conn) dtlsPackRecord(epoch *epochState, typ recordType, data []byte, mustPack bool) (n int, num DTLSRecordNumber, err error) {
	maxLen := c.config.Bugs.MaxHandshakeRecordLength
	if maxLen <= 0 {
		maxLen = 1024
	}

	vers := c.wireVersion
	if vers == 0 {

		if c.isDTLS {
			vers = VersionDTLS10
		} else {
			vers = VersionTLS10
		}
	}
	if c.vers >= VersionTLS13 || c.out.version >= VersionTLS13 {
		vers = VersionDTLS12
	}

	useDTLS13RecordHeader := c.out.version >= VersionTLS13 && epoch.cipher != nil && !c.useDTLSPlaintextHeader()
	headerHasLength := true
	record := make([]byte, 0, dtlsMaxRecordHeaderLen+len(data)+c.out.maxEncryptOverhead(epoch, len(data)))
	seq := c.out.sequenceNumberForOutput(epoch)
	if useDTLS13RecordHeader {
		record = c.appendDTLS13RecordHeader(record, seq, len(data))
		headerHasLength = !c.config.DTLSRecordHeaderOmitLength
	} else {
		record = append(record, byte(typ))
		record = append(record, byte(vers>>8))
		record = append(record, byte(vers))

		record = append(record, seq...)
		record = append(record, byte(len(data)>>8))
		record = append(record, byte(len(data)))
	}

	recordHeaderLen := len(record)
	record, err = c.out.encrypt(epoch, record, data, typ, recordHeaderLen, headerHasLength)
	if err != nil {
		return
	}
	num = c.out.lastRecordNumber(epoch, true            )

	if useDTLS13RecordHeader && !c.config.Bugs.NullAllCiphers {
		sample := record[recordHeaderLen:]
		mask := epoch.recordNumberEncrypter.generateMask(sample)
		seqLen := 2
		if c.config.DTLSUseShortSeqNums {
			seqLen = 1
		}

		xorSlice(record[1:1+seqLen], mask)
	}

	if !mustPack && len(record)+len(c.pendingPacket) > c.config.Bugs.PackHandshakeRecords {
		err = c.dtlsFlushPacket()
		if err != nil {
			return
		}
	}

	c.pendingPacket = append(c.pendingPacket, record...)
	if c.config.DTLSRecordHeaderOmitLength {
		if c.config.Bugs.SplitAndPackAppData {
			panic("incompatible config")
		}
		err = c.dtlsFlushPacket()
		if err != nil {
			return
		}
	}
	n = len(data)
	return
}

func (c *Conn) dtlsFlushPacket() error {
	if c.hand.Len() == 0 {
		c.lastRecordInFlight = nil
	}
	if len(c.pendingPacket) == 0 {
		return nil
	}
	_, err := c.conn.Write(c.pendingPacket)
	c.pendingPacket = nil
	return err
}

func readDTLSFragment(s *cryptobyte.String) (DTLSFragment, error) {
	var f DTLSFragment
	var totLen, fragOffset uint32
	if !s.ReadUint8(&f.Type) ||
		!s.ReadUint24(&totLen) ||
		!s.ReadUint16(&f.Sequence) ||
		!s.ReadUint24(&fragOffset) ||
		!readUint24LengthPrefixedBytes(s, &f.Data) {
		return DTLSFragment{}, errors.New("dtls: bad handshake record")
	}
	f.TotalLength = int(totLen)
	f.Offset = int(fragOffset)
	if f.Offset > f.TotalLength || len(f.Data) > f.TotalLength-f.Offset {
		return DTLSFragment{}, errors.New("dtls: bad fragment offset")
	}

	if len(f.Data) == 0 && f.TotalLength != 0 {
		return DTLSFragment{}, errors.New("dtls: fragment makes no progress")
	}
	return f, nil
}

func (c *Conn) makeDTLSRecordNumberInfo(epoch *epochState, data []byte) (DTLSRecordNumberInfo, error) {
	info := DTLSRecordNumberInfo{DTLSRecordNumber: c.in.lastRecordNumber(epoch, false            )}

	s := cryptobyte.String(data)
	first := true
	for !s.Empty() {
		f, err := readDTLSFragment(&s)
		if err != nil {
			return DTLSRecordNumberInfo{}, err
		}

		if first {
			info.MessageStartSequence = f.Sequence
			info.MessageStartOffset = f.Offset
			first = false
		}
		info.MessageEndSequence = f.Sequence
		info.MessageEndOffset = f.Offset + len(f.Data)
	}

	return info, nil
}

func (c *Conn) dtlsDoReadHandshake() ([]byte, error) {

	for len(c.handMsg) < 4+c.handMsgLen {

		if c.hand.Len() == 0 {
			if err := c.in.err; err != nil {
				return nil, err
			}
			if err := c.readRecord(recordTypeHandshake); err != nil {
				return nil, err
			}
		}

		s := cryptobyte.String(c.hand.Bytes())
		f, err := readDTLSFragment(&s)
		if err != nil {
			return nil, err
		}
		c.hand.Next(c.hand.Len() - len(s))

		if f.Sequence != c.recvHandshakeSeq {
			return nil, errors.New("dtls: bad handshake sequence number")
		}

		if c.handMsg == nil {
			c.handMsgLen = f.TotalLength
			if c.handMsgLen > maxHandshake {
				return nil, c.in.setErrorLocked(c.sendAlert(alertInternalError))
			}

			c.handMsg = []byte{f.Type, byte(f.TotalLength >> 16), byte(f.TotalLength >> 8), byte(f.TotalLength)}
		} else if f.TotalLength != c.handMsgLen {
			return nil, errors.New("dtls: bad handshake length")
		}

		if 4+f.Offset != len(c.handMsg) {
			return nil, errors.New("dtls: bad fragment offset")
		}

		c.handMsg = append(c.handMsg, f.Data...)
		if len(c.handMsg) < 4+c.handMsgLen {
			if c.hand.Len() != 0 {
				return nil, errors.New("dtls: truncated handshake fragment was not last in the record")
			}
			if c.lastRecordInFlight.bytesAvailable > 0 {
				return nil, fmt.Errorf("dtls: handshake fragment was truncated, but record could have fit %d more bytes", c.lastRecordInFlight.bytesAvailable)
			}
		}

		c.expectedACK = nil
	}
	c.recvHandshakeSeq++
	ret := c.handMsg
	c.handMsg, c.handMsgLen = nil, 0
	c.receivedFlight = append(c.receivedFlight, DTLSMessage{
		Epoch:    c.in.epoch.epoch,
		Type:     ret[0],
		Sequence: c.recvHandshakeSeq - 1,
		Data:     ret[4:],
	})
	return ret, nil
}

func (c *Conn) checkACK(data []byte) error {
	s := cryptobyte.String(data)
	var child cryptobyte.String
	if !s.ReadUint16LengthPrefixed(&child) || !s.Empty() {
		return fmt.Errorf("tls: could not parse ACK record")
	}

	var acks []DTLSRecordNumber
	for !child.Empty() {
		var num DTLSRecordNumber
		if !child.ReadUint64(&num.Epoch) || !child.ReadUint64(&num.Sequence) {
			return fmt.Errorf("tls: could not parse ACK record")
		}
		acks = append(acks, num)
	}

	expected := c.expectedACK
	if len(expected) > shimConfig.MaxACKBuffer {
		expected = expected[len(expected)-shimConfig.MaxACKBuffer:]
	}

	if c.maxPacketLen != 0 && len(acks) > 10 && len(acks) < len(expected) {
		expected = expected[len(expected)-len(acks):]
	}

	expected = slices.Clone(expected)
	slices.SortFunc(expected, func(a, b DTLSRecordNumber) int {
		cmp1 := cmp.Compare(a.Epoch, b.Epoch)
		if cmp1 != 0 {
			return cmp1
		}
		return cmp.Compare(a.Sequence, b.Sequence)
	})

	if !slices.Equal(acks, expected) {
		return fmt.Errorf("tls: got ACKs %+v, but expected %+v", acks, expected)
	}

	return nil
}

func DTLSServer(conn net.Conn, config *Config) *Conn {
	c := &Conn{config: config, isDTLS: true, conn: conn}
	c.init()
	return c
}

func DTLSClient(conn net.Conn, config *Config) *Conn {
	c := &Conn{config: config, isClient: true, isDTLS: true, conn: conn}
	c.init()
	return c
}

type WriteFlightFunc = func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo)
type ACKFlightFunc = func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo)

type DTLSController struct {
	conn *Conn
	err  error

	retransmitNeeded    []DTLSFragment
	mergeIntoNextFlight bool
}

func newDTLSController(conn *Conn, received []DTLSMessage) DTLSController {
	var retransmitNeeded []DTLSFragment
	for i := range received {
		msg := &received[i]
		retransmitNeeded = append(retransmitNeeded, msg.Fragment(0, len(msg.Data)))
	}

	return DTLSController{conn: conn, retransmitNeeded: retransmitNeeded}
}

func (c *DTLSController) getOutEpochOrPanic(epochValue uint16) *epochState {
	epoch, ok := c.conn.out.getEpoch(epochValue)
	if !ok {
		panic(fmt.Sprintf("tls: could not find epoch %d", epochValue))
	}
	return epoch
}

func (c *DTLSController) getInEpochOrPanic(epochValue uint16) *epochState {
	epoch, ok := c.conn.in.getEpoch(epochValue)
	if !ok {
		panic(fmt.Sprintf("tls: could not find epoch %d", epochValue))
	}
	return epoch
}

func (c *DTLSController) Err() error { return c.err }

func (c *DTLSController) OutEpoch() uint16 {
	return c.conn.out.epoch.epoch
}

func (c *DTLSController) InEpoch() uint16 {
	return c.conn.in.epoch.epoch
}

func (c *DTLSController) AdvanceClock(duration time.Duration) {
	if c.err != nil {
		return
	}

	c.err = c.conn.dtlsFlushPacket()
	if c.err != nil {
		return
	}

	adaptor := c.conn.config.Bugs.PacketAdaptor
	if adaptor == nil {
		panic("tls: no PacketAdapter set")
	}

	received, err := adaptor.SendReadTimeout(duration)
	if err != nil {
		c.err = err
	} else if len(received) != 0 {
		c.err = fmt.Errorf("tls: received %d unexpected packets while simulating a timeout", len(received))
	}
}

func (c *DTLSController) SetMTU(mtu int) {
	if c.err != nil {
		return
	}

	adaptor := c.conn.config.Bugs.PacketAdaptor
	if adaptor == nil {
		panic("tls: no PacketAdapter set")
	}

	c.conn.maxPacketLen = mtu
	c.err = adaptor.SetPeerMTU(mtu)
}

func (c *DTLSController) WriteFlight(msgs []DTLSMessage) {
	config := c.conn.config
	if c.err != nil {
		return
	}

	var fragments []DTLSFragment

	for _, msg := range msgs {
		if msg.IsChangeCipherSpec {
			fragments = append(fragments, msg.Fragment(0, len(msg.Data)))
			continue
		}

		maxLen := config.Bugs.MaxHandshakeRecordLength
		if maxLen <= 0 {
			maxLen = 1024
		}

		if config.Bugs.SendEmptyFragments {
			fragments = append(fragments, msg.Fragment(0, 0))
			fragments = append(fragments, msg.Fragment(len(msg.Data), 0))
		}

		firstRun := true
		fragOffset := 0
		for firstRun || fragOffset < len(msg.Data) {
			firstRun = false
			fragLen := min(len(msg.Data)-fragOffset, maxLen)

			fragment := msg.Fragment(fragOffset, fragLen)
			fragments = append(fragments, fragment)
			if config.Bugs.ReorderHandshakeFragments {

				if msg.Type != typeFinished {
					fragments = append(fragments, fragment)
				}

				if fragLen > (maxLen+1)/2 {

					fragLen = (maxLen + 1) / 2
				}
			}
			fragOffset += fragLen
		}
		if config.Bugs.MixCompleteMessageWithFragments {
			fragments = append(fragments, msg.Fragment(0, len(msg.Data)))
		}
	}

	for start := 0; start < len(fragments); {
		end := start + 1
		for end < len(fragments) && fragments[start].Epoch == fragments[end].Epoch {
			end++
		}
		chunk := fragments[start:end]
		if config.Bugs.ReorderHandshakeFragments {
			rand.Shuffle(len(chunk), func(i, j int) { chunk[i], chunk[j] = chunk[j], chunk[i] })
		}
		start = end
	}

	c.WriteFragments(fragments)
}

func (c *DTLSController) WriteFragments(fragments []DTLSFragment) {
	config := c.conn.config
	if c.err != nil {
		return
	}

	maxRecordLen := config.Bugs.PackHandshakeFragments
	packRecord := func(epoch *epochState, typ recordType, data []byte, anyDiscard bool) error {
		_, num, err := c.conn.dtlsPackRecord(epoch, typ, data, false)
		if err != nil {
			return err
		}
		if !anyDiscard && typ == recordTypeHandshake {
			c.conn.expectedACK = append(c.conn.expectedACK, num)
		}
		return nil
	}

	var record []byte
	var epoch *epochState
	var anyDiscard bool
	flush := func() error {
		if len(record) > 0 {
			if err := packRecord(epoch, recordTypeHandshake, record, anyDiscard); err != nil {
				return err
			}
		}
		record = nil
		anyDiscard = false
		return nil
	}

	for i := range fragments {
		f := &fragments[i]
		if epoch != nil && (f.Epoch != epoch.epoch || f.IsChangeCipherSpec) {
			c.err = flush()
			if c.err != nil {
				return
			}
			epoch = nil
		}

		if epoch == nil {
			epoch = c.getOutEpochOrPanic(f.Epoch)
		}

		if f.IsChangeCipherSpec {
			c.err = packRecord(epoch, recordTypeChangeCipherSpec, f.Bytes(), false)
			if c.err != nil {
				return
			}
			continue
		}

		fBytes := f.Bytes()
		if n := config.Bugs.SplitFragments; n > 0 {
			if len(fBytes) > n {
				c.err = packRecord(epoch, recordTypeHandshake, fBytes[:n], f.ShouldDiscard)
				if c.err != nil {
					return
				}
				c.err = packRecord(epoch, recordTypeHandshake, fBytes[n:], f.ShouldDiscard)
				if c.err != nil {
					return
				}
			} else {
				c.err = packRecord(epoch, recordTypeHandshake, fBytes, f.ShouldDiscard)
				if c.err != nil {
					return
				}
			}
		} else {
			if len(record)+len(fBytes) > maxRecordLen {
				c.err = flush()
				if c.err != nil {
					return
				}
			}
			if f.ShouldDiscard {
				anyDiscard = true
			}
			record = append(record, fBytes...)
		}
	}

	c.err = flush()
}

func (c *DTLSController) WriteACK(epoch uint16, records []DTLSRecordNumberInfo) {
	if c.err != nil {
		return
	}

	ack := cryptobyte.NewBuilder(make([]byte, 0, 2+8*len(records)))
	ack.AddUint16LengthPrefixed(func(recordNumbers *cryptobyte.Builder) {
		for _, r := range records {
			recordNumbers.AddUint64(r.Epoch)
			recordNumbers.AddUint64(r.Sequence)
		}
	})
	_, _, c.err = c.conn.dtlsPackRecord(c.getOutEpochOrPanic(epoch), recordTypeACK, ack.BytesOrPanic(), false)
	if c.err != nil {
		return
	}

	for _, r := range records {
		if !r.HasACKInformation() {
			continue
		}
		var update []DTLSFragment
		for _, f := range c.retransmitNeeded {
			endOffset := f.Offset + len(f.Data)

			if comparePair(f.Sequence, f.Offset, r.MessageStartSequence, r.MessageStartOffset) < 0 {

				if comparePair(f.Sequence, endOffset, r.MessageStartSequence, r.MessageStartOffset) <= 0 {

					update = append(update, f)
				} else {

					prefix := f
					prefix.Data = f.Data[:r.MessageStartOffset-f.Offset]
					update = append(update, prefix)
				}
			}

			if comparePair(r.MessageEndSequence, r.MessageEndOffset, f.Sequence, endOffset) < 0 {

				if comparePair(r.MessageEndSequence, r.MessageEndOffset, f.Sequence, f.Offset) <= 0 {

					update = append(update, f)
				} else {

					suffix := f
					suffix.Offset = r.MessageEndOffset
					suffix.Data = f.Data[r.MessageEndOffset-f.Offset:]
					update = append(update, suffix)
				}
			}
		}
		c.retransmitNeeded = update
	}
}

func (c *DTLSController) ReadRetransmit() []DTLSRecordNumberInfo {
	if c.err != nil {
		return nil
	}

	var ret []DTLSRecordNumberInfo
	ret, c.err = c.doReadRetransmit()
	return ret
}

func (c *DTLSController) doReadRetransmit() ([]DTLSRecordNumberInfo, error) {
	if err := c.conn.dtlsFlushPacket(); err != nil {
		return nil, err
	}

	var records []DTLSRecordNumberInfo
	expected := slices.Clone(c.retransmitNeeded)
	for len(expected) > 0 {

		wantTyp := recordTypeHandshake
		if expected[0].IsChangeCipherSpec {
			wantTyp = recordTypeChangeCipherSpec
		}
		epoch := c.getInEpochOrPanic(expected[0].Epoch)

		c.conn.skipRecordVersionCheck = !expected[0].IsChangeCipherSpec && expected[0].Type == typeClientHello
		typ, data, err := c.conn.dtlsDoReadRecord(epoch, wantTyp)
		c.conn.skipRecordVersionCheck = false
		if err != nil {
			return nil, err
		}
		if typ != wantTyp {
			return nil, fmt.Errorf("tls: got record of type %d in retransmit, but expected %d", typ, wantTyp)
		}
		if typ == recordTypeChangeCipherSpec {
			if len(data) != 1 || data[0] != 1 {
				return nil, errors.New("tls: got invalid ChangeCipherSpec")
			}
			expected = expected[1:]
			continue
		}

		s := cryptobyte.String(data)
		if s.Empty() {
			return nil, fmt.Errorf("tls: got empty record in retransmit")
		}
		for !s.Empty() {
			if len(expected) == 0 || expected[0].Epoch != epoch.epoch || expected[0].IsChangeCipherSpec {
				return nil, fmt.Errorf("tls: got excess data at epoch %d in retransmit", epoch.epoch)
			}

			exp := &expected[0]
			var f DTLSFragment
			f, err = readDTLSFragment(&s)
			if f.Type != exp.Type || f.TotalLength != exp.TotalLength || f.Sequence != exp.Sequence || f.Offset != exp.Offset {
				return nil, fmt.Errorf("tls: got offset %d of message %d (type %d, length %d), expected offset %d of message %d (type %d, length %d)", f.Offset, f.Sequence, f.Type, f.TotalLength, exp.Offset, exp.Sequence, exp.Type, exp.TotalLength)
			}
			if len(f.Data) > len(exp.Data) {
				return nil, fmt.Errorf("tls: got %d bytes at offset %d of message %d but only %d bytes were missing", len(f.Data), f.Offset, f.Sequence, len(exp.Data))
			}
			if !bytes.Equal(f.Data, exp.Data[:len(f.Data)]) {
				return nil, fmt.Errorf("tls: got %d bytes at offset %d of message %d but did not match original", len(f.Data), f.Offset, f.Sequence)
			}
			if len(f.Data) == len(exp.Data) {
				expected = expected[1:]
			} else {

				exp.Offset += len(f.Data)
				exp.Data = exp.Data[len(f.Data):]

				if !s.Empty() {
					return nil, errors.New("dtls: truncated handshake fragment was not last in the record")
				}
				if c.conn.lastRecordInFlight.bytesAvailable > 0 {
					return nil, fmt.Errorf("dtls: handshake fragment was truncated, but record could have fit %d more bytes", c.conn.lastRecordInFlight.bytesAvailable)
				}
			}
		}

		record, err := c.conn.makeDTLSRecordNumberInfo(epoch, data)
		if err != nil {
			return nil, err
		}
		records = append(records, record)
	}
	return records, nil
}

func (c *DTLSController) ReadACK(epoch uint16) {
	if c.err != nil {
		return
	}

	c.err = c.conn.dtlsFlushPacket()
	if c.err != nil {
		return
	}

	typ, data, err := c.conn.dtlsDoReadRecord(c.getInEpochOrPanic(epoch), recordTypeACK)
	if err != nil {
		c.err = err
		return
	}
	if typ != recordTypeACK {
		c.err = fmt.Errorf("tls: got record of type %d, but expected ACK", typ)
		return
	}

	c.err = c.conn.checkACK(data)
}

func (c *DTLSController) WriteAppData(epoch uint16, data []byte) {
	if c.err != nil {
		return
	}

	_, _, c.err = c.conn.dtlsPackRecord(c.getOutEpochOrPanic(epoch), recordTypeApplicationData, data, false)
}

func (c *DTLSController) ReadAppData(epoch uint16, expected []byte) {
	if c.err != nil {
		return
	}

	if err := c.conn.dtlsFlushPacket(); err != nil {
		c.err = err
		return
	}

	typ, data, err := c.conn.dtlsDoReadRecord(c.getInEpochOrPanic(epoch), recordTypeApplicationData)
	if err != nil {
		c.err = err
		return
	}
	if typ != recordTypeApplicationData {
		c.err = fmt.Errorf("tls: got record of type %d, but expected application data", typ)
		return
	}
	if !bytes.Equal(data, expected) {
		c.err = fmt.Errorf("tls: got app data record containing %x, but expected %x", data, expected)
		return
	}
}

func (c *DTLSController) ExpectNextTimeout(d time.Duration) {
	if c.err != nil {
		return
	}
	if err := c.conn.dtlsFlushPacket(); err != nil {
		c.err = err
		return
	}
	c.err = c.conn.config.Bugs.PacketAdaptor.ExpectNextTimeout(d)
}

func (c *DTLSController) ExpectNoNextTimeout() {
	if c.err != nil {
		return
	}
	if err := c.conn.dtlsFlushPacket(); err != nil {
		c.err = err
		return
	}
	c.err = c.conn.config.Bugs.PacketAdaptor.ExpectNoNextTimeout()
}

func (c *DTLSController) MergeIntoNextFlight() {
	c.mergeIntoNextFlight = true
}
