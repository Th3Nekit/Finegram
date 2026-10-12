// Copyright 2016 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package runner

import (
	"bytes"
	"crypto"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/rsa"
	"crypto/x509"
	_ "embed"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"slices"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"boringssl.googlesource.com/boringssl.git/util/testresult"
	"golang.org/x/crypto/cryptobyte"
)

var (
	useValgrind        = flag.Bool("valgrind", false, "If true, run code under valgrind")
	useGDB             = flag.Bool("gdb", false, "If true, run BoringSSL code under gdb")
	useLLDB            = flag.Bool("lldb", false, "If true, run BoringSSL code under lldb")
	useRR              = flag.Bool("rr-record", false, "If true, run BoringSSL code under `rr record`.")
	waitForDebugger    = flag.Bool("wait-for-debugger", false, "If true, jobs will run one at a time and pause for a debugger to attach")
	flagDebug          = flag.Bool("debug", false, "Hexdump the contents of the connection")
	mallocTest         = flag.Int64("malloc-test", -1, "If non-negative, run each test with each malloc in turn failing from the given number onwards.")
	mallocTestDebug    = flag.Bool("malloc-test-debug", false, "If true, ask bssl_shim to abort rather than fail a malloc. This can be used with a specific value for --malloc-test to identity the malloc failing that is causing problems.")
	jsonOutput         = flag.String("json-output", "", "The file to output JSON results to.")
	pipe               = flag.Bool("pipe", false, "If true, print status output suitable for piping into another program.")
	testToRun          = flag.String("test", "", "Semicolon-separated patterns of tests to run, or empty to run all tests")
	skipTest           = flag.String("skip", "", "Semicolon-separated patterns of tests to skip")
	allowHintMismatch  = flag.String("allow-hint-mismatch", "", "Semicolon-separated patterns of tests where hints may mismatch")
	numWorkersFlag     = flag.Int("num-workers", runtime.NumCPU(), "The number of workers to run in parallel.")
	shimPath           = flag.String("shim-path", "../../../build/ssl/test/bssl_shim", "The location of the shim binary.")
	shimExtraFlags     = flag.String("shim-extra-flags", "", "Semicolon-separated extra flags to pass to the shim binary on each invocation.")
	handshakerPath     = flag.String("handshaker-path", "../../../build/ssl/test/handshaker", "The location of the handshaker binary.")
	fuzzer             = flag.Bool("fuzzer", false, "If true, tests against a BoringSSL built in fuzzer mode.")
	transcriptDir      = flag.String("transcript-dir", "", "The directory in which to write transcripts.")
	idleTimeout        = flag.Duration("idle-timeout", 15*time.Second, "The number of seconds to wait for a read or write to bssl_shim.")
	deterministic      = flag.Bool("deterministic", false, "If true, uses a deterministic PRNG in the runner.")
	allowUnimplemented = flag.Bool("allow-unimplemented", false, "If true, report pass even if some tests are unimplemented.")
	looseErrors        = flag.Bool("loose-errors", false, "If true, allow shims to report an untranslated error code.")
	shimConfigFile     = flag.String("shim-config", "", "A config file to use to configure the tests for this shim.")
	includeDisabled    = flag.Bool("include-disabled", false, "If true, also runs disabled tests.")
	repeatUntilFailure = flag.Bool("repeat-until-failure", false, "If true, the first selected test will be run repeatedly until failure.")
)

type ShimConfiguration struct {

	DisabledTests map[string]string

	ErrorMap map[string][]string

	HalfRTTTickets int

	AllCurves []int

	MaxACKBuffer int
}

var shimConfig ShimConfiguration = ShimConfiguration{
	HalfRTTTickets: 2,
	MaxACKBuffer:   32,
}

var rsa2048KeyPEM []byte

var rsa1024KeyPEM []byte

var ecdsaP224KeyPEM []byte

var ecdsaP256KeyPEM []byte

var ecdsaP384KeyPEM []byte

var ecdsaP521KeyPEM []byte

var ed25519KeyPEM []byte

var channelIDKeyPEM []byte

var (
	rsa1024Key rsa.PrivateKey
	rsa2048Key rsa.PrivateKey

	ecdsaP224Key ecdsa.PrivateKey
	ecdsaP256Key ecdsa.PrivateKey
	ecdsaP384Key ecdsa.PrivateKey
	ecdsaP521Key ecdsa.PrivateKey

	ed25519Key ed25519.PrivateKey

	channelIDKey ecdsa.PrivateKey
)

var channelIDKeyPath string

func initKeys() {

	for _, k := range []struct {
		pemBytes []byte
		key      *rsa.PrivateKey
	}{
		{rsa1024KeyPEM, &rsa1024Key},
		{rsa2048KeyPEM, &rsa2048Key},
	} {
		key, err := loadPEMKey(k.pemBytes)
		if err != nil {
			panic(fmt.Sprintf("failed to load RSA test key: %s", err))
		}
		*k.key = *(key.(*rsa.PrivateKey))
	}

	for _, k := range []struct {
		pemBytes []byte
		key      *ecdsa.PrivateKey
	}{
		{ecdsaP224KeyPEM, &ecdsaP224Key},
		{ecdsaP256KeyPEM, &ecdsaP256Key},
		{ecdsaP384KeyPEM, &ecdsaP384Key},
		{ecdsaP521KeyPEM, &ecdsaP521Key},
		{channelIDKeyPEM, &channelIDKey},
	} {
		key, err := loadPEMKey(k.pemBytes)
		if err != nil {
			panic(fmt.Sprintf("failed to load ECDSA test key: %s", err))
		}
		*k.key = *(key.(*ecdsa.PrivateKey))
	}

	k, err := loadPEMKey(ed25519KeyPEM)
	if err != nil {
		panic(fmt.Sprintf("failed to load Ed25519 test key: %s", err))
	}
	ed25519Key = k.(ed25519.PrivateKey)

	channelIDKeyPath = writeTempKeyFile(&channelIDKey)
}

var channelIDBytes []byte

var (
	testOCSPResponse  = []byte{1, 2, 3, 4}
	testOCSPResponse2 = []byte{5, 6, 7, 8}
	testSCTList       = []byte{0, 6, 0, 4, 5, 6, 7, 8}
	testSCTList2      = []byte{0, 6, 0, 4, 1, 2, 3, 4}
)

var (
	testOCSPExtension = append([]byte{byte(extensionStatusRequest) >> 8, byte(extensionStatusRequest), 0, 8, statusTypeOCSP, 0, 0, 4}, testOCSPResponse...)
	testSCTExtension  = append([]byte{byte(extensionSignedCertificateTimestamp) >> 8, byte(extensionSignedCertificateTimestamp), 0, byte(len(testSCTList))}, testSCTList...)
)

var (
	rsaCertificate       Credential
	rsaChainCertificate  Credential
	rsa1024Certificate   Credential
	ecdsaP224Certificate Credential
	ecdsaP256Certificate Credential
	ecdsaP384Certificate Credential
	ecdsaP521Certificate Credential
	ed25519Certificate   Credential
	garbageCertificate   Credential
)

func initCertificates() {
	for _, def := range []struct {
		name string
		key  crypto.Signer
		out  *Credential
	}{
		{"Test RSA-1024 Cert", &rsa1024Key, &rsa1024Certificate},
		{"Test RSA-2048 Cert", &rsa2048Key, &rsaCertificate},
		{"Test ECDSA P-224 Cert", &ecdsaP224Key, &ecdsaP224Certificate},
		{"Test ECDSA P-256 Cert", &ecdsaP256Key, &ecdsaP256Certificate},
		{"Test ECDSA P-384 Cert", &ecdsaP384Key, &ecdsaP384Certificate},
		{"Test ECDSA P-521 Cert", &ecdsaP521Key, &ecdsaP521Certificate},
		{"Test Ed25519 Cert", ed25519Key, &ed25519Certificate},
	} {
		template := *baseCertTemplate
		template.Subject.CommonName = def.name
		*def.out = generateSingleCertChain(&template, def.key)
	}

	channelIDBytes = make([]byte, 64)
	writeIntPadded(channelIDBytes[:32], channelIDKey.X)
	writeIntPadded(channelIDBytes[32:], channelIDKey.Y)

	garbageCertificate.Certificate = [][]byte{[]byte("GARBAGE")}
	garbageCertificate.PrivateKey = rsaCertificate.PrivateKey

	rootTmpl := *baseCertTemplate
	rootTmpl.Subject.CommonName = "test root"
	rootCert := generateTestCert(&rootTmpl, nil, &rsa2048Key)
	intermediateTmpl := *baseCertTemplate
	intermediateTmpl.Subject.CommonName = "test inter"
	intermediateCert := generateTestCert(&intermediateTmpl, rootCert, &rsa2048Key)
	leafTmpl := *baseCertTemplate
	leafTmpl.IsCA, leafTmpl.BasicConstraintsValid = false, false
	leafCert := generateTestCert(nil, intermediateCert, &rsa2048Key)

	keyPath := writeTempKeyFile(&rsa2048Key)
	rootCertPath, chainPath := writeTempCertFile([]*x509.Certificate{rootCert}), writeTempCertFile([]*x509.Certificate{leafCert, intermediateCert})

	rsaChainCertificate = Credential{
		Certificate:     [][]byte{leafCert.Raw, intermediateCert.Raw},
		RootCertificate: rootCert.Raw,
		PrivateKey:      &rsa2048Key,
		Leaf:            leafCert,
		ChainPath:       chainPath,
		KeyPath:         keyPath,
		RootPath:        rootCertPath,
	}
}

func flagInts(flagName string, vals []int) []string {
	ret := make([]string, 0, 2*len(vals))
	for _, val := range vals {
		ret = append(ret, flagName, strconv.Itoa(val))
	}
	return ret
}

func base64FlagValue(in []byte) string {
	return base64.StdEncoding.EncodeToString(in)
}

func useDebugger() bool {
	return *useGDB || *useLLDB || *useRR || *waitForDebugger
}

func loadPEMKey(pemBytes []byte) (crypto.PrivateKey, error) {
	block, _ := pem.Decode(pemBytes)
	if block == nil {
		return nil, fmt.Errorf("no PEM block found")
	}

	if block.Type != "PRIVATE KEY" {
		return nil, fmt.Errorf("unexpected PEM type (expected \"PRIVATE KEY\"): %s", block.Type)
	}

	k, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		return nil, fmt.Errorf("failed to parse PKCS#8 key: %s", err)
	}

	return k, nil
}

func recordVersionToWire(vers uint16, protocol protocol) uint16 {
	if protocol == dtls {
		switch vers {
		case VersionTLS12:
			return VersionDTLS12
		case VersionTLS10:
			return VersionDTLS10
		}
	} else {
		switch vers {
		case VersionSSL30, VersionTLS10, VersionTLS11, VersionTLS12:
			return vers
		}
	}

	panic("unknown version")
}

func encodeDERValues(values [][]byte) string {
	var ret string
	for i, v := range values {
		if i > 0 {
			ret += ","
		}
		ret += hex.EncodeToString(v)
	}

	return ret
}

func decodeHexOrPanic(in string) []byte {
	ret, err := hex.DecodeString(in)
	if err != nil {
		panic(err)
	}
	return ret
}

type testType int

const (
	clientTest testType = iota
	serverTest
)

func (t testType) String() string {
	switch t {
	case clientTest:
		return "Client"
	case serverTest:
		return "Server"
	}
	panic(fmt.Sprintf("bad test type %d", t))
}

type protocol int

const (
	tls protocol = iota
	dtls
	quic
)

func (p protocol) String() string {
	switch p {
	case tls:
		return "TLS"
	case dtls:
		return "DTLS"
	case quic:
		return "QUIC"
	}
	return "unknown protocol"
}

const (
	alpn = 1
	npn  = 2
)

type connectionExpectations struct {

	version uint16

	cipher uint16

	channelID bool

	nextProto string

	noNextProto bool

	nextProtoType int

	srtpProtectionProfile uint16

	peerSignatureAlgorithm signatureAlgorithm

	curveID CurveID

	peerCertificate *Credential

	quicTransportParams []byte

	quicTransportParamsLegacy []byte

	peerApplicationSettings []byte

	peerApplicationSettingsOld []byte

	echAccepted bool
}

type testCase struct {
	testType      testType
	protocol      protocol
	name          string
	config        Config
	shouldFail    bool
	expectedError string

	expectedLocalError string

	expectations connectionExpectations

	resumeExpectations *connectionExpectations

	messageLen int

	messageCount int

	resumeSession bool

	resumeRenewedSession bool

	expectResumeRejected bool

	resumeConfig *Config

	newSessionsOnResume bool

	noSessionCache bool

	sendPrefix string

	shimWritesFirst bool

	readWithUnfinishedWrite bool

	shimShutsDown bool

	renegotiate int

	sendHalfHelloRequest bool

	renegotiateCiphers []uint16

	replayWrites bool

	damageFirstWrite bool

	exportKeyingMaterial int
	exportLabel          string
	exportContext        string
	useExportContext     bool

	flags []string

	testTLSUnique bool

	sendEmptyRecords int

	sendWarningAlerts int

	sendUserCanceledAlerts int

	sendBogusAlertType bool

	sendKeyUpdates int

	keyUpdateRequest byte

	shimSendsKeyUpdateBeforeRead bool

	expectUnsolicitedKeyUpdate bool

	expectMessageDropped bool

	shimPrefix string

	resumeShimPrefix string

	exportTrafficSecrets bool

	skipTransportParamsConfig bool

	skipQUICALPNConfig bool

	earlyData bool

	expectEarlyDataRejected bool

	skipSplitHandshake bool

	skipHints bool

	skipVersionNameCheck bool

	shimCertificate *Credential

	handshakerCertificate *Credential

	shimCredentials       []*Credential
	resumeShimCredentials []*Credential
}

var testCases []testCase

func appendTranscript(path string, data []byte) error {
	if len(data) == 0 {
		return nil
	}

	settings, err := os.ReadFile(path)
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}

		settings = []byte{0, 0}
	}

	settings = append(settings, data...)
	return os.WriteFile(path, settings, 0644)
}

type timeoutConn struct {
	net.Conn
	timeout time.Duration
}

func (t *timeoutConn) Read(b []byte) (int, error) {
	if !*useGDB {
		if err := t.SetReadDeadline(time.Now().Add(t.timeout)); err != nil {
			return 0, err
		}
	}
	return t.Conn.Read(b)
}

func (t *timeoutConn) Write(b []byte) (int, error) {
	if !*useGDB {
		if err := t.SetWriteDeadline(time.Now().Add(t.timeout)); err != nil {
			return 0, err
		}
	}
	return t.Conn.Write(b)
}

func makeTestMessage(msgIdx int, messageLen int) []byte {
	testMessage := make([]byte, messageLen)
	for i := range testMessage {
		testMessage[i] = 0x42 ^ byte(msgIdx)
	}
	return testMessage
}

func expectedReply(b []byte) []byte {
	ret := make([]byte, len(b))
	for i, v := range b {
		ret[i] = v ^ 0xff
	}
	return ret
}

func doExchange(test *testCase, config *Config, conn net.Conn, isResume bool, transcripts *[][]byte, num int) error {
	if !test.noSessionCache {
		if config.ClientSessionCache == nil {
			config.ClientSessionCache = NewLRUClientSessionCache(1)
		}
		if config.ServerSessionCache == nil {
			config.ServerSessionCache = NewLRUServerSessionCache(1)
		}
	}
	if test.testType != clientTest {

		if len(config.ServerName) == 0 {
			config.ServerName = "test"
		}
	}

	if *fuzzer {
		config.Bugs.NullAllCiphers = true
	}
	if *deterministic {
		config.Time = func() time.Time { return time.Unix(1234, 1234) }
	}

	if !useDebugger() {
		conn = &timeoutConn{conn, *idleTimeout}
	}

	if test.protocol == dtls {
		config.Bugs.PacketAdaptor = newPacketAdaptor(conn)
		conn = config.Bugs.PacketAdaptor
	}

	if *flagDebug || len(*transcriptDir) != 0 {
		local, peer := "client", "server"
		if test.testType == clientTest {
			local, peer = peer, local
		}
		connDebug := &recordingConn{
			Conn:       conn,
			isDatagram: test.protocol == dtls,
			local:      local,
			peer:       peer,
		}
		conn = connDebug
		if *flagDebug {
			defer connDebug.WriteTo(os.Stdout)
		}
		if len(*transcriptDir) != 0 {
			defer func() {
				if num == len(*transcripts) {
					*transcripts = append(*transcripts, connDebug.Transcript())
				} else {
					panic("transcripts are out of sync")
				}
			}()

			var clientHelloCount int
			config.Bugs.RecordClientHelloInner = func(encodedInner, outer []byte) error {
				name := fmt.Sprintf("%s-%d-%d", test.name, num, clientHelloCount)
				clientHelloCount++
				dir := filepath.Join(*transcriptDir, "decode_client_hello_inner")
				if err := os.MkdirAll(dir, 0755); err != nil {
					return err
				}
				bb := cryptobyte.NewBuilder(nil)
				addUint24LengthPrefixedBytes(bb, encodedInner)
				bb.AddBytes(outer)
				return os.WriteFile(filepath.Join(dir, name), bb.BytesOrPanic(), 0644)
			}
		}

		if config.Bugs.PacketAdaptor != nil {
			config.Bugs.PacketAdaptor.debug = connDebug
		}
	}
	if test.protocol == quic {
		config.Bugs.MockQUICTransport = newMockQUICTransport(conn)

		conn = config.Bugs.MockQUICTransport
	}

	if test.replayWrites {
		conn = newReplayAdaptor(conn)
	}

	var connDamage *damageAdaptor
	if test.damageFirstWrite {
		connDamage = newDamageAdaptor(conn)
		conn = connDamage
	}

	if test.sendPrefix != "" {
		if _, err := conn.Write([]byte(test.sendPrefix)); err != nil {
			return err
		}
	}

	var tlsConn *Conn
	if test.testType == clientTest {
		if test.protocol == dtls {
			tlsConn = DTLSServer(conn, config)
		} else {
			tlsConn = Server(conn, config)
		}
	} else {
		config.InsecureSkipVerify = true
		if test.protocol == dtls {
			tlsConn = DTLSClient(conn, config)
		} else {
			tlsConn = Client(conn, config)
		}
	}
	defer tlsConn.Close()

	if err := tlsConn.Handshake(); err != nil {
		return err
	}

	expectations := &test.expectations
	if isResume && test.resumeExpectations != nil {
		expectations = test.resumeExpectations
	}
	connState := tlsConn.ConnectionState()
	if vers := connState.Version; expectations.version != 0 && vers != expectations.version {
		return fmt.Errorf("got version %x, expected %x", vers, expectations.version)
	}

	if cipher := connState.CipherSuite; expectations.cipher != 0 && cipher != expectations.cipher {
		return fmt.Errorf("got cipher %x, expected %x", cipher, expectations.cipher)
	}
	if didResume := connState.DidResume; isResume && didResume == test.expectResumeRejected {
		return fmt.Errorf("didResume is %t, but we expected the opposite", didResume)
	}

	if expectations.channelID {
		channelID := connState.ChannelID
		if channelID == nil {
			return fmt.Errorf("no channel ID negotiated")
		}
		if channelID.Curve != channelIDKey.Curve ||
			channelIDKey.X.Cmp(channelIDKey.X) != 0 ||
			channelIDKey.Y.Cmp(channelIDKey.Y) != 0 {
			return fmt.Errorf("incorrect channel ID")
		}
	} else if connState.ChannelID != nil {
		return fmt.Errorf("channel ID unexpectedly negotiated")
	}

	if expected := expectations.nextProto; expected != "" {
		if actual := connState.NegotiatedProtocol; actual != expected {
			return fmt.Errorf("next proto mismatch: got %s, wanted %s", actual, expected)
		}
	}

	if expectations.noNextProto {
		if actual := connState.NegotiatedProtocol; actual != "" {
			return fmt.Errorf("got unexpected next proto %s", actual)
		}
	}

	if expectations.nextProtoType != 0 {
		if (expectations.nextProtoType == alpn) != connState.NegotiatedProtocolFromALPN {
			return fmt.Errorf("next proto type mismatch")
		}
	}

	if expectations.peerApplicationSettings != nil {
		if !connState.HasApplicationSettings {
			return errors.New("application settings should have been negotiated")
		}
		if !bytes.Equal(connState.PeerApplicationSettings, expectations.peerApplicationSettings) {
			return fmt.Errorf("peer application settings mismatch: got %q, wanted %q", connState.PeerApplicationSettings, expectations.peerApplicationSettings)
		}
	} else if connState.HasApplicationSettings {
		return errors.New("application settings unexpectedly negotiated")
	}

	if expectations.peerApplicationSettingsOld != nil {
		if !connState.HasApplicationSettingsOld {
			return errors.New("old application settings should have been negotiated")
		}
		if !bytes.Equal(connState.PeerApplicationSettingsOld, expectations.peerApplicationSettingsOld) {
			return fmt.Errorf("old peer application settings mismatch: got %q, wanted %q", connState.PeerApplicationSettingsOld, expectations.peerApplicationSettingsOld)
		}
	} else if connState.HasApplicationSettingsOld {
		return errors.New("old application settings unexpectedly negotiated")
	}

	if p := connState.SRTPProtectionProfile; p != expectations.srtpProtectionProfile {
		return fmt.Errorf("SRTP profile mismatch: got %d, wanted %d", p, expectations.srtpProtectionProfile)
	}

	if expected := expectations.peerSignatureAlgorithm; expected != 0 && expected != connState.PeerSignatureAlgorithm {
		return fmt.Errorf("expected peer to use signature algorithm %04x, but got %04x", expected, connState.PeerSignatureAlgorithm)
	}

	if expected := expectations.curveID; expected != 0 && expected != connState.CurveID {
		return fmt.Errorf("expected peer to use curve %04x, but got %04x", expected, connState.CurveID)
	}

	if expected := expectations.peerCertificate; expected != nil {
		if len(connState.PeerCertificates) != len(expected.Certificate) {
			return fmt.Errorf("expected peer to send %d certificates, but got %d", len(connState.PeerCertificates), len(expected.Certificate))
		}
		for i, cert := range connState.PeerCertificates {
			if !bytes.Equal(cert.Raw, expected.Certificate[i]) {
				return fmt.Errorf("peer certificate %d did not match", i+1)
			}
		}

		if !bytes.Equal(connState.OCSPResponse, expected.OCSPStaple) {
			return fmt.Errorf("peer OCSP response did not match")
		}

		if !bytes.Equal(connState.SCTList, expected.SignedCertificateTimestampList) {
			return fmt.Errorf("peer SCT list did not match")
		}

		if expected.Type == CredentialTypeDelegated {
			if connState.PeerDelegatedCredential == nil {
				return fmt.Errorf("peer unexpectedly did not use delegated credentials")
			}
			if !bytes.Equal(expected.DelegatedCredential, connState.PeerDelegatedCredential) {
				return fmt.Errorf("peer delegated credential did not match")
			}
		} else if connState.PeerDelegatedCredential != nil {
			return fmt.Errorf("peer unexpectedly used delegated credentials")
		}
	}

	if len(expectations.quicTransportParams) > 0 {
		if !bytes.Equal(expectations.quicTransportParams, connState.QUICTransportParams) {
			return errors.New("Peer did not send expected QUIC transport params")
		}
	}

	if len(expectations.quicTransportParamsLegacy) > 0 {
		if !bytes.Equal(expectations.quicTransportParamsLegacy, connState.QUICTransportParamsLegacy) {
			return errors.New("Peer did not send expected legacy QUIC transport params")
		}
	}

	if expectations.echAccepted {
		if !connState.ECHAccepted {
			return errors.New("tls: server did not accept ECH")
		}
	} else {
		if connState.ECHAccepted {
			return errors.New("tls: server unexpectedly accepted ECH")
		}
	}

	if test.exportKeyingMaterial > 0 {
		actual := make([]byte, test.exportKeyingMaterial)
		if _, err := io.ReadFull(tlsConn, actual); err != nil {
			return err
		}
		expected, err := tlsConn.ExportKeyingMaterial(test.exportKeyingMaterial, []byte(test.exportLabel), []byte(test.exportContext), test.useExportContext)
		if err != nil {
			return err
		}
		if !bytes.Equal(actual, expected) {
			return fmt.Errorf("keying material mismatch; got %x, wanted %x", actual, expected)
		}
	}

	if test.exportTrafficSecrets {
		secretLenBytes := make([]byte, 2)
		if _, err := io.ReadFull(tlsConn, secretLenBytes); err != nil {
			return err
		}
		secretLen := binary.LittleEndian.Uint16(secretLenBytes)

		theirReadSecret := make([]byte, secretLen)
		theirWriteSecret := make([]byte, secretLen)
		if _, err := io.ReadFull(tlsConn, theirReadSecret); err != nil {
			return err
		}
		if _, err := io.ReadFull(tlsConn, theirWriteSecret); err != nil {
			return err
		}

		myReadSecret := tlsConn.in.trafficSecret
		myWriteSecret := tlsConn.out.trafficSecret
		if !bytes.Equal(myWriteSecret, theirReadSecret) {
			return fmt.Errorf("read traffic-secret mismatch; got %x, wanted %x", theirReadSecret, myWriteSecret)
		}
		if !bytes.Equal(myReadSecret, theirWriteSecret) {
			return fmt.Errorf("write traffic-secret mismatch; got %x, wanted %x", theirWriteSecret, myReadSecret)
		}
	}

	if test.testTLSUnique {
		var peersValue [12]byte
		if _, err := io.ReadFull(tlsConn, peersValue[:]); err != nil {
			return err
		}
		expected := tlsConn.ConnectionState().TLSUnique
		if !bytes.Equal(peersValue[:], expected) {
			return fmt.Errorf("tls-unique mismatch: peer sent %x, but %x was expected", peersValue[:], expected)
		}
	}

	if test.sendHalfHelloRequest {
		tlsConn.SendHalfHelloRequest()
	}

	shimPrefix := test.shimPrefix
	if isResume {
		shimPrefix = test.resumeShimPrefix
	}
	if test.shimWritesFirst || test.readWithUnfinishedWrite {
		shimPrefix = shimInitialWrite
	}
	if test.renegotiate > 0 {

		if shimPrefix != "" && !test.readWithUnfinishedWrite {
			buf := make([]byte, len(shimPrefix))
			_, err := io.ReadFull(tlsConn, buf)
			if err != nil {
				return err
			}
			if string(buf) != shimPrefix {
				return fmt.Errorf("bad initial message %v vs %v", string(buf), shimPrefix)
			}
			shimPrefix = ""
		}

		if test.renegotiateCiphers != nil {
			config.CipherSuites = test.renegotiateCiphers
		}
		for i := 0; i < test.renegotiate; i++ {
			if err := tlsConn.Renegotiate(); err != nil {
				return err
			}
		}
	} else if test.renegotiateCiphers != nil {
		panic("renegotiateCiphers without renegotiate")
	}

	if test.damageFirstWrite {
		connDamage.setDamage(true)
		if _, err := tlsConn.Write([]byte("DAMAGED WRITE")); err != nil {
			return err
		}
		connDamage.setDamage(false)
	}

	messageLen := test.messageLen
	if messageLen < 0 {
		if test.protocol == dtls {
			return fmt.Errorf("messageLen < 0 not supported for DTLS tests")
		}

		_, err := io.Copy(io.Discard, tlsConn)
		return err
	}
	if messageLen == 0 {
		messageLen = 32
	}

	messageCount := test.messageCount

	if messageCount == 0 && !test.shimShutsDown {
		messageCount = 1
	}

	for j := 0; j < messageCount; j++ {
		for i := 0; i < test.sendKeyUpdates; i++ {
			if err := tlsConn.SendKeyUpdate(test.keyUpdateRequest); err != nil {
				return err
			}
		}

		for i := 0; i < test.sendEmptyRecords; i++ {
			if _, err := tlsConn.Write(nil); err != nil {
				return err
			}
		}

		for i := 0; i < test.sendWarningAlerts; i++ {
			if err := tlsConn.SendAlert(alertLevelWarning, alertUnexpectedMessage); err != nil {
				return err
			}
		}

		for i := 0; i < test.sendUserCanceledAlerts; i++ {
			if err := tlsConn.SendAlert(alertLevelWarning, alertUserCanceled); err != nil {
				return err
			}
		}

		if test.sendBogusAlertType {
			if err := tlsConn.SendAlert(0x42, alertUnexpectedMessage); err != nil {
				return err
			}
		}

		if test.shimSendsKeyUpdateBeforeRead {
			if err := tlsConn.ReadKeyUpdate(); err != nil {
				return err
			}
		}

		testMessage := makeTestMessage(j, messageLen)
		if _, err := tlsConn.Write(testMessage); err != nil {
			return err
		}

		if shimPrefix != "" {
			buf := make([]byte, len(shimPrefix))
			_, err := io.ReadFull(tlsConn, buf)
			if err != nil {
				return err
			}
			if string(buf) != shimPrefix {
				return fmt.Errorf("bad initial message %v vs %v", string(buf), shimPrefix)
			}
			shimPrefix = ""
		}

		if test.shimShutsDown || test.expectMessageDropped {

			continue
		}

		if test.sendKeyUpdates > 0 && test.keyUpdateRequest == keyUpdateRequested {
			if err := tlsConn.ReadKeyUpdate(); err != nil {
				return err
			}
		}

		buf := make([]byte, len(testMessage))
		if test.protocol == dtls {
			bufTmp := make([]byte, len(buf)+1)
			n, err := tlsConn.Read(bufTmp)
			if err != nil {
				return err
			}
			if config.Bugs.SplitAndPackAppData {
				m, err := tlsConn.Read(bufTmp[n:])
				if err != nil {
					return err
				}
				n += m
			}
			if n != len(buf) {
				return fmt.Errorf("bad reply; length mismatch (%d vs %d)", n, len(buf))
			}
			copy(buf, bufTmp)
		} else {
			_, err := io.ReadFull(tlsConn, buf)
			if err != nil {
				return err
			}
		}

		if expected := expectedReply(testMessage); !bytes.Equal(buf, expected) {
			return fmt.Errorf("bad reply contents; got %x and wanted %x", buf, expected)
		}

		if seen := tlsConn.keyUpdateSeen; seen != test.expectUnsolicitedKeyUpdate {
			return fmt.Errorf("keyUpdateSeen (%t) != expectUnsolicitedKeyUpdate", seen)
		}
	}

	if test.shimSendsKeyUpdateBeforeRead {
		if err := tlsConn.ReadKeyUpdate(); err != nil {
			return err
		}
	}

	return nil
}

const xtermSize = "140x50"

func valgrindOf(dbAttach bool, path string, args ...string) *exec.Cmd {
	valgrindArgs := []string{"--error-exitcode=99", "--track-origins=yes", "--leak-check=full", "--quiet"}
	if dbAttach {
		valgrindArgs = append(valgrindArgs, "--db-attach=yes", "--db-command=xterm -geometry "+xtermSize+" -e gdb -nw %f %p")
	}
	valgrindArgs = append(valgrindArgs, path)
	valgrindArgs = append(valgrindArgs, args...)

	return exec.Command("valgrind", valgrindArgs...)
}

func gdbOf(path string, args ...string) *exec.Cmd {
	xtermArgs := []string{"-geometry", xtermSize, "-e", "gdb", "--args"}
	xtermArgs = append(xtermArgs, path)
	xtermArgs = append(xtermArgs, args...)

	return exec.Command("xterm", xtermArgs...)
}

func lldbOf(path string, args ...string) *exec.Cmd {
	xtermArgs := []string{"-geometry", xtermSize, "-e", "lldb", "--"}
	xtermArgs = append(xtermArgs, path)
	xtermArgs = append(xtermArgs, args...)

	return exec.Command("xterm", xtermArgs...)
}

func rrOf(path string, args ...string) *exec.Cmd {
	rrArgs := []string{"record", path}
	rrArgs = append(rrArgs, args...)
	return exec.Command("rr", rrArgs...)
}

func removeFirstLineIfSuffix(s, suffix string) string {
	idx := strings.IndexByte(s, '\n')
	if idx < 0 {
		return s
	}
	if strings.HasSuffix(s[:idx], suffix) {
		return s[idx+1:]
	}
	return s
}

var (
	errMoreMallocs   = errors.New("child process did not exhaust all allocation calls")
	errUnimplemented = errors.New("child process does not implement needed flags")
)

type shimProcess struct {
	cmd *exec.Cmd

	done           chan struct{}
	childErr       error
	listener       *shimListener
	stdout, stderr bytes.Buffer
}

func newShimProcess(dispatcher *shimDispatcher, shimPath string, flags []string, env []string) (*shimProcess, error) {
	listener, err := dispatcher.NewShim()
	if err != nil {
		return nil, err
	}

	shim := &shimProcess{listener: listener}
	cmdFlags := []string{
		"-port", strconv.Itoa(listener.Port()),
		"-shim-id", strconv.FormatUint(listener.ShimID(), 10),
	}
	if listener.IsIPv6() {
		cmdFlags = append(cmdFlags, "-ipv6")
	}
	cmdFlags = append(cmdFlags, flags...)

	if *useValgrind {
		shim.cmd = valgrindOf(false, shimPath, cmdFlags...)
	} else if *useGDB {
		shim.cmd = gdbOf(shimPath, cmdFlags...)
	} else if *useLLDB {
		shim.cmd = lldbOf(shimPath, cmdFlags...)
	} else if *useRR {
		shim.cmd = rrOf(shimPath, cmdFlags...)
	} else {
		shim.cmd = exec.Command(shimPath, cmdFlags...)
	}
	shim.cmd.Stdin = os.Stdin
	shim.cmd.Stdout = &shim.stdout
	shim.cmd.Stderr = &shim.stderr
	shim.cmd.Env = env

	if err := shim.cmd.Start(); err != nil {
		shim.listener.Close()
		return nil, err
	}

	shim.done = make(chan struct{})
	go func() {
		shim.childErr = shim.cmd.Wait()
		shim.listener.Close()
		close(shim.done)
	}()
	return shim, nil
}

func (s *shimProcess) accept() (net.Conn, error) {
	var deadline time.Time
	if !useDebugger() {
		deadline = time.Now().Add(*idleTimeout)
	}
	return s.listener.Accept(deadline)
}

func (s *shimProcess) wait() error {

	s.listener.Close()

	if !useDebugger() {
		waitTimeout := time.AfterFunc(*idleTimeout, func() {
			s.cmd.Process.Kill()
		})
		defer waitTimeout.Stop()
	}

	<-s.done
	return s.childErr
}

func (s *shimProcess) close() {
	s.listener.Close()
	s.cmd.Process.Kill()
}

func doExchanges(test *testCase, shim *shimProcess, resumeCount int, transcripts *[][]byte) error {
	config := test.config
	if *deterministic {
		config.Rand = &deterministicRand{}
	}

	conn, err := shim.accept()
	if err != nil {
		return err
	}
	err = doExchange(test, &config, conn, false                       , transcripts, 0)
	conn.Close()
	if err != nil {
		return err
	}

	nextTicketKey := config.SessionTicketKey
	for i := 0; i < resumeCount; i++ {
		var resumeConfig Config
		if test.resumeConfig != nil {
			resumeConfig = *test.resumeConfig
			resumeConfig.Rand = config.Rand
			if resumeConfig.Credential == nil {
				resumeConfig.Credential = config.Credential
			}
		} else {
			resumeConfig = config
		}

		if test.newSessionsOnResume {
			resumeConfig.ClientSessionCache = nil
			resumeConfig.ServerSessionCache = nil
			if _, err := resumeConfig.rand().Read(resumeConfig.SessionTicketKey[:]); err != nil {
				return err
			}
		} else {
			resumeConfig.ClientSessionCache = config.ClientSessionCache
			resumeConfig.ServerSessionCache = config.ServerSessionCache

			resumeConfig.SessionTicketKey = nextTicketKey
			if _, err := resumeConfig.rand().Read(nextTicketKey[:]); err != nil {
				return err
			}
			resumeConfig.Bugs.EncryptSessionTicketKey = &nextTicketKey
		}

		var connResume net.Conn
		connResume, err = shim.accept()
		if err != nil {
			return err
		}
		err = doExchange(test, &resumeConfig, connResume, true                 , transcripts, i+1)
		connResume.Close()
		if err != nil {
			return err
		}
	}

	return nil
}

func translateExpectedError(canonical string) []string {
	if translated, found := shimConfig.ErrorMap[canonical]; found {
		return translated
	}

	return []string{canonical}
}

func formatErrors(expectedErrors []string, stderr, local, child, stdout, expectedLocal, expectedCanonical string) (string, string) {
	got := fmt.Sprintf("\tstderr:\n\t\t%s\n\tlocal: %q\n\tchild: %q\n\tstdout: %s", stderr, local, child, stdout)
	want := fmt.Sprintf("\tlocal: %q\n\tremote: %q", expectedLocal, expectedCanonical)
	if slices.Equal(expectedErrors, []string{expectedCanonical}) {
		return got, want
	}
	if len(expectedErrors) == 0 || expectedErrors == nil {
		return got, want + " (no specified mapping)"
	}
	return got, want + " mapped to one of:\n\t\t" + strings.Join(expectedErrors, "\n\t\t")
}

func matchError(expectedErrors []string, stderr string) bool {
	for _, expectedError := range expectedErrors {
		if strings.Contains(stderr, expectedError) {
			return true
		}
	}
	return false
}

const shimInitialWrite = "hello"

func appendCredentialFlags(flags []string, cred *Credential, prefix string, newCredential bool) []string {
	if newCredential {
		switch cred.Type {
		case CredentialTypeX509:
			flags = append(flags, prefix+"-new-x509-credential")
		case CredentialTypeDelegated:
			flags = append(flags, prefix+"-new-delegated-credential")
		case CredentialTypeSPAKE2PlusV1:
			flags = append(flags, prefix+"-new-spake2plusv1-credential")
		default:
			panic(fmt.Errorf("unknown credential type %d", cred.Type))
		}
	} else if cred.Type != CredentialTypeX509 {
		panic("default credential must be X.509")
	}

	handleBase64Field := func(flag string, value []byte) {
		if len(value) != 0 {
			flags = append(flags, fmt.Sprintf("%s-%s", prefix, flag), base64FlagValue(value))
		}
	}

	if len(cred.ChainPath) != 0 {
		flags = append(flags, prefix+"-cert-file", cred.ChainPath)
	}
	if len(cred.KeyPath) != 0 {
		flags = append(flags, prefix+"-key-file", cred.KeyPath)
	}
	handleBase64Field("ocsp-response", cred.OCSPStaple)
	handleBase64Field("signed-cert-timestamps", cred.SignedCertificateTimestampList)
	for _, sigAlg := range cred.SignatureAlgorithms {
		flags = append(flags, prefix+"-signing-prefs", strconv.Itoa(int(sigAlg)))
	}
	handleBase64Field("delegated-credential", cred.DelegatedCredential)
	if cred.MustMatchIssuer {
		flags = append(flags, prefix+"-must-match-issuer")
	}
	handleBase64Field("pake-context", cred.PAKEContext)
	handleBase64Field("pake-client-id", cred.PAKEClientID)
	handleBase64Field("pake-server-id", cred.PAKEServerID)
	handleBase64Field("pake-password", cred.PAKEPassword)
	if cred.WrongPAKERole {
		flags = append(flags, prefix+"-wrong-pake-role")
	}
	handleBase64Field("trust-anchor-id", cred.TrustAnchorID)
	return flags
}

func runTest(dispatcher *shimDispatcher, statusChan chan statusMsg, test *testCase, shimPath string, mallocNumToFail int64) error {

	defer func() {
		if r := recover(); r != nil {
			fmt.Fprintf(os.Stderr, "Test '%s' panicked.\n", test.name)
			panic(r)
		}
	}()

	var flags []string
	if len(*shimExtraFlags) > 0 {
		flags = strings.Split(*shimExtraFlags, ";")
	}
	if *fuzzer {
		flags = append(flags, "-fuzzer-mode")
	}
	if test.testType == serverTest {
		flags = append(flags, "-server")
	}

	shimCertificate := test.shimCertificate
	if shimCertificate == nil && len(test.shimCredentials) == 0 && len(test.resumeShimCredentials) == 0 && test.testType == serverTest && len(test.config.PreSharedKey) == 0 {
		shimCertificate = &rsaCertificate
	}
	if shimCertificate != nil {
		var shimPrefix string
		if test.handshakerCertificate != nil {
			shimPrefix = "-on-shim"
		}
		flags = appendCredentialFlags(flags, shimCertificate, shimPrefix, false)
	}
	if test.handshakerCertificate != nil {
		flags = appendCredentialFlags(flags, test.handshakerCertificate, "-on-handshaker", false)
	}

	for _, cred := range test.shimCredentials {
		flags = appendCredentialFlags(flags, cred, "", true)
	}
	for _, cred := range test.resumeShimCredentials {
		flags = appendCredentialFlags(flags, cred, "-on-resume", true)
	}

	if test.protocol == dtls {
		flags = append(flags, "-dtls")
	} else if test.protocol == quic {
		flags = append(flags, "-quic")
		if !test.skipTransportParamsConfig {
			test.config.QUICTransportParams = []byte{1, 2}
			test.config.QUICTransportParamsUseLegacyCodepoint = QUICUseCodepointStandard
			if test.resumeConfig != nil {
				test.resumeConfig.QUICTransportParams = []byte{1, 2}
				test.resumeConfig.QUICTransportParamsUseLegacyCodepoint = QUICUseCodepointStandard
			}
			test.expectations.quicTransportParams = []byte{3, 4}
			if test.resumeExpectations != nil {
				test.resumeExpectations.quicTransportParams = []byte{3, 4}
			}
			useCodepointFlag := "0"
			if test.config.QUICTransportParamsUseLegacyCodepoint == QUICUseCodepointLegacy {
				useCodepointFlag = "1"
			}
			flags = append(flags,
				"-quic-transport-params",
				base64FlagValue([]byte{3, 4}),
				"-expect-quic-transport-params",
				base64FlagValue([]byte{1, 2}),
				"-quic-use-legacy-codepoint", useCodepointFlag)
		}
		if !test.skipQUICALPNConfig {
			flags = append(flags,
				[]string{
					"-advertise-alpn", "\x03foo",
					"-select-alpn", "foo",
					"-expect-alpn", "foo",
				}...)
			test.config.NextProtos = []string{"foo"}
			if test.resumeConfig != nil {
				test.resumeConfig.NextProtos = []string{"foo"}
			}
			test.expectations.nextProto = "foo"
			test.expectations.nextProtoType = alpn
			if test.resumeExpectations != nil {
				test.resumeExpectations.nextProto = "foo"
				test.resumeExpectations.nextProtoType = alpn
			}
		}
	}

	if test.earlyData {
		if !test.resumeSession {
			panic("earlyData set without resumeSession in " + test.name)
		}

		resumeConfig := test.resumeConfig
		if resumeConfig == nil {
			resumeConfig = &test.config
		}
		if test.expectEarlyDataRejected {
			flags = append(flags, "-on-resume-expect-reject-early-data")
		} else {
			flags = append(flags, "-on-resume-expect-accept-early-data")
		}

		if test.protocol == quic {

			flags = append(flags, "-quic-early-data-context", "context")
		}

		flags = append(flags, "-enable-early-data")
		if test.testType == clientTest {

			flags = append(flags, "-expect-ticket-supports-early-data")
			if test.config.MaxEarlyDataSize == 0 {
				test.config.MaxEarlyDataSize = 16384
			}
			if resumeConfig.MaxEarlyDataSize == 0 {
				resumeConfig.MaxEarlyDataSize = 16384
			}

			if test.protocol != dtls {

				flags = append(flags, "-on-resume-shim-writes-first")
				if resumeConfig.Bugs.ExpectEarlyData == nil {
					resumeConfig.Bugs.ExpectEarlyData = [][]byte{[]byte(shimInitialWrite)}
				}
			}
		} else {

			if resumeConfig.Bugs.SendEarlyData == nil {
				resumeConfig.Bugs.SendEarlyData = [][]byte{{1, 2, 3, 4}}
			}
			if resumeConfig.Bugs.ExpectHalfRTTData == nil {
				resumeConfig.Bugs.ExpectHalfRTTData = [][]byte{{254, 253, 252, 251}}
			}
			resumeConfig.Bugs.ExpectEarlyDataAccepted = !test.expectEarlyDataRejected
		}
	}

	var resumeCount int
	if test.resumeSession {
		resumeCount++
		if test.resumeRenewedSession {
			resumeCount++
		}
	}

	if resumeCount > 0 {
		flags = append(flags, "-resume-count", strconv.Itoa(resumeCount))
	}

	if test.shimWritesFirst {
		flags = append(flags, "-shim-writes-first")
	}

	if test.readWithUnfinishedWrite {
		flags = append(flags, "-read-with-unfinished-write")
	}

	if test.shimShutsDown {
		flags = append(flags, "-shim-shuts-down")
	}

	if test.exportKeyingMaterial > 0 {
		flags = append(flags, "-export-keying-material", strconv.Itoa(test.exportKeyingMaterial))
		if test.useExportContext {
			flags = append(flags, "-use-export-context")
		}
	}
	if test.exportKeyingMaterial > 0 {
		flags = append(flags, "-export-label", test.exportLabel)
		flags = append(flags, "-export-context", test.exportContext)
	}

	if test.exportTrafficSecrets {
		flags = append(flags, "-export-traffic-secrets")
	}

	if test.expectResumeRejected {
		flags = append(flags, "-expect-session-miss")
	}

	if test.testTLSUnique {
		flags = append(flags, "-tls-unique")
	}

	if test.shimSendsKeyUpdateBeforeRead {
		flags = append(flags, "-key-update-before-read")
	}

	if *waitForDebugger {
		flags = append(flags, "-wait-for-debugger")
	}

	var transcriptPrefix string
	var transcripts [][]byte
	if len(*transcriptDir) != 0 {
		protocol := "tls"
		if test.protocol == dtls {
			protocol = "dtls"
		} else if test.protocol == quic {
			protocol = "quic"
		}

		side := "client"
		if test.testType == serverTest {
			side = "server"
		}

		dir := filepath.Join(*transcriptDir, protocol, side)
		if err := os.MkdirAll(dir, 0755); err != nil {
			return err
		}
		transcriptPrefix = filepath.Join(dir, test.name+"-")
		flags = append(flags, "-write-settings", transcriptPrefix)
	}

	if test.testType == clientTest && test.config.Credential == nil {
		test.config.Credential = &rsaCertificate
	}
	if test.config.Credential != nil {
		flags = append(flags, "-trust-cert", test.config.Credential.RootPath)
	}

	flags = append(flags, test.flags...)

	var env []string
	if mallocNumToFail >= 0 {
		env = os.Environ()
		env = append(env, "MALLOC_NUMBER_TO_FAIL="+strconv.FormatInt(mallocNumToFail, 10))
		if *mallocTestDebug {
			env = append(env, "MALLOC_BREAK_ON_FAIL=1")
		}
		env = append(env, "_MALLOC_CHECK=1")
	}

	shim, err := newShimProcess(dispatcher, shimPath, flags, env)
	if err != nil {
		return err
	}
	statusChan <- statusMsg{test: test, statusType: statusShimStarted, pid: shim.cmd.Process.Pid}
	defer shim.close()

	localErr := doExchanges(test, shim, resumeCount, &transcripts)
	childErr := shim.wait()

	for i, transcript := range transcripts {
		if err := appendTranscript(transcriptPrefix+strconv.Itoa(i), transcript); err != nil {
			return err
		}
	}

	var isValgrindError, mustFail bool
	if exitError, ok := childErr.(*exec.ExitError); ok {
		switch exitError.Sys().(syscall.WaitStatus).ExitStatus() {
		case 88:
			return errMoreMallocs
		case 89:
			return errUnimplemented
		case 90:
			mustFail = true
		case 99:
			isValgrindError = true
		}
	}

	stdout := strings.Replace(shim.stdout.String(), "\r\n", "\n", -1)
	stderr := strings.Replace(shim.stderr.String(), "\r\n", "\n", -1)

	stderr = removeFirstLineIfSuffix(stderr, ": unsupported flags DT_FLAGS_1=0x8000001")

	var extraStderr string
	if stderrParts := strings.SplitN(stderr, "--- DONE ---\n", 2); len(stderrParts) == 2 {
		stderr = stderrParts[0]
		extraStderr = stderrParts[1]
	}

	failed := localErr != nil || childErr != nil
	expectedErrors := translateExpectedError(test.expectedError)
	correctFailure := *looseErrors || matchError(expectedErrors, stderr)

	localErrString := "none"
	if localErr != nil {
		localErrString = localErr.Error()
	}
	if len(test.expectedLocalError) != 0 {
		correctFailure = correctFailure && strings.Contains(localErrString, test.expectedLocalError)
	}

	if failed != test.shouldFail || failed && !correctFailure || mustFail {
		childErrString := "none"
		if childErr != nil {
			childErrString = childErr.Error()
		}

		got, want := formatErrors(expectedErrors,
			stderr, localErrString, childErrString, stdout,
			test.expectedLocalError, test.expectedError)

		var msg string
		switch {
		case failed && !test.shouldFail:
			msg = fmt.Sprintf("unexpected failure\ngot:\n%s\n", got)
		case !failed && test.shouldFail:
			msg = fmt.Sprintf("unexpected success\nwant:\n%s\n", want)
		case failed && !correctFailure:
			msg = fmt.Sprintf("unexpected error\ngot:\n%s\n\nwant:\n%s\n", got, want)
		case mustFail:
			msg = "test failure"
		default:
			panic("internal error")
		}

		return fmt.Errorf("%s\nextra stderr:\n%s", msg, extraStderr)
	}

	if len(extraStderr) > 0 || (!failed && len(stderr) > 0) {
		return fmt.Errorf("unexpected error output:\n%s\n%s", stderr, extraStderr)
	}

	if *useValgrind && isValgrindError {
		return fmt.Errorf("valgrind error:\n%s\n%s", stderr, extraStderr)
	}

	return nil
}

type tlsVersion struct {
	name string

	version uint16

	excludeFlag string
	hasDTLS     bool
	hasQUIC     bool

	versionDTLS uint16

	versionWire uint16
}

func (vers tlsVersion) String() string {
	return vers.name
}

func (vers tlsVersion) shimFlag(protocol protocol) string {

	if protocol == dtls && vers.versionDTLS != 0 {
		return strconv.Itoa(int(vers.versionDTLS))
	}
	return strconv.Itoa(int(vers.version))
}

func (vers tlsVersion) wire(protocol protocol) uint16 {
	if protocol == dtls && vers.versionDTLS != 0 {
		return vers.versionDTLS
	}
	if vers.versionWire != 0 {
		return vers.versionWire
	}
	return vers.version
}

func (vers tlsVersion) supportsProtocol(protocol protocol) bool {
	if protocol == dtls {
		return vers.hasDTLS
	}
	if protocol == quic {
		return vers.hasQUIC
	}
	return true
}

var tlsVersions = []tlsVersion{
	{
		name:        "TLS1",
		version:     VersionTLS10,
		excludeFlag: "-no-tls1",
		hasDTLS:     true,
		versionDTLS: VersionDTLS10,
	},
	{
		name:        "TLS11",
		version:     VersionTLS11,
		excludeFlag: "-no-tls11",
	},
	{
		name:        "TLS12",
		version:     VersionTLS12,
		excludeFlag: "-no-tls12",
		hasDTLS:     true,
		versionDTLS: VersionDTLS12,
	},
	{
		name:        "TLS13",
		version:     VersionTLS13,
		excludeFlag: "-no-tls13",
		hasQUIC:     true,
		hasDTLS:     true,
		versionDTLS: VersionDTLS13,
		versionWire: VersionTLS13,
	},
}

func allVersions(protocol protocol) []tlsVersion {
	if protocol == tls {
		return tlsVersions
	}

	var ret []tlsVersion
	for _, vers := range tlsVersions {
		if vers.supportsProtocol(protocol) {
			ret = append(ret, vers)
		}
	}
	return ret
}

func convertToSplitHandshakeTests(tests []testCase) (splitHandshakeTests []testCase, err error) {
	var stdout bytes.Buffer
	var flags []string
	if len(*shimExtraFlags) > 0 {
		flags = strings.Split(*shimExtraFlags, ";")
	}
	flags = append(flags, "-is-handshaker-supported")
	shim := exec.Command(*shimPath, flags...)
	shim.Stdout = &stdout
	if err := shim.Run(); err != nil {
		return nil, err
	}

	switch strings.TrimSpace(string(stdout.Bytes())) {
	case "No":
		return
	case "Yes":
		break
	default:
		return nil, fmt.Errorf("unknown output from shim: %q", stdout.Bytes())
	}

	var allowHintMismatchPattern []string
	if len(*allowHintMismatch) > 0 {
		allowHintMismatchPattern = strings.Split(*allowHintMismatch, ";")
	}

NextTest:
	for _, test := range tests {
		if test.protocol != tls ||
			test.testType != serverTest ||
			len(test.shimCredentials) != 0 ||
			len(test.resumeShimCredentials) != 0 ||
			strings.Contains(test.name, "ECH-Server") ||
			test.skipSplitHandshake {
			continue
		}

		for _, flag := range test.flags {
			if flag == "-implicit-handshake" {
				continue NextTest
			}
		}

		shTest := test
		shTest.name += "-Split"
		shTest.flags = make([]string, len(test.flags), len(test.flags)+3)
		copy(shTest.flags, test.flags)
		shTest.flags = append(shTest.flags, "-handoff", "-handshaker-path", *handshakerPath)

		splitHandshakeTests = append(splitHandshakeTests, shTest)
	}

	for _, test := range tests {
		if test.protocol == dtls ||
			test.testType != serverTest ||
			test.skipHints {
			continue
		}

		var matched bool
		if len(allowHintMismatchPattern) > 0 {
			matched, err = match(allowHintMismatchPattern, nil, test.name)
			if err != nil {
				return nil, fmt.Errorf("error matching pattern: %s", err)
			}
		}

		shTest := test
		shTest.name += "-Hints"
		shTest.flags = make([]string, len(test.flags), len(test.flags)+3)
		copy(shTest.flags, test.flags)
		shTest.flags = append(shTest.flags, "-handshake-hints", "-handshaker-path", *handshakerPath)
		if matched {
			shTest.flags = append(shTest.flags, "-allow-hint-mismatch")
		}

		splitHandshakeTests = append(splitHandshakeTests, shTest)
	}

	return splitHandshakeTests, nil
}

func worker(dispatcher *shimDispatcher, statusChan chan statusMsg, c chan *testCase, shimPath string, wg *sync.WaitGroup) {
	defer wg.Done()

	for test := range c {
		var err error

		if *mallocTest >= 0 {
			for mallocNumToFail := int64(*mallocTest); ; mallocNumToFail++ {
				statusChan <- statusMsg{test: test, statusType: statusStarted}
				if err = runTest(dispatcher, statusChan, test, shimPath, mallocNumToFail); err != errMoreMallocs {
					if err != nil {
						fmt.Printf("\n\nmalloc test failed at %d: %s\n", mallocNumToFail, err)
					}
					break
				}
			}
		} else if *repeatUntilFailure {
			for err == nil {
				statusChan <- statusMsg{test: test, statusType: statusStarted}
				err = runTest(dispatcher, statusChan, test, shimPath, -1)
			}
		} else {
			statusChan <- statusMsg{test: test, statusType: statusStarted}
			err = runTest(dispatcher, statusChan, test, shimPath, -1)
		}
		statusChan <- statusMsg{test: test, statusType: statusDone, err: err}
	}
}

type statusType int

const (
	statusStarted statusType = iota
	statusShimStarted
	statusDone
)

type statusMsg struct {
	test       *testCase
	statusType statusType
	pid        int
	err        error
}

func statusPrinter(doneChan chan *testresult.Results, statusChan chan statusMsg, total int) {
	var started, done, failed, unimplemented, lineLen int

	testOutput := testresult.NewResults()
	for msg := range statusChan {
		if !*pipe {

			var erase string
			for i := 0; i < lineLen; i++ {
				erase += "\b \b"
			}
			fmt.Print(erase)
		}

		if msg.statusType == statusStarted {
			started++
		} else if msg.statusType == statusDone {
			done++

			if msg.err != nil {
				if msg.err == errUnimplemented {
					if *pipe {

						fmt.Printf("UNIMPLEMENTED (%s)\n", msg.test.name)
					}
					unimplemented++
					if *allowUnimplemented {
						testOutput.AddSkip(msg.test.name)
					} else {
						testOutput.AddResult(msg.test.name, "SKIP", nil)
					}
				} else {
					fmt.Printf("FAILED (%s)\n%s\n", msg.test.name, msg.err)
					failed++
					testOutput.AddResult(msg.test.name, "FAIL", msg.err)
				}
			} else {
				if *pipe {

					fmt.Printf("PASSED (%s)\n", msg.test.name)
				}
				testOutput.AddResult(msg.test.name, "PASS", nil)
			}
		}

		if !*pipe {

			line := fmt.Sprintf("%d/%d/%d/%d/%d", failed, unimplemented, done, started, total)
			if msg.statusType == statusShimStarted && *waitForDebugger {

				line += fmt.Sprintf(" (%s: attach to process %d to continue)", msg.test.name, msg.pid)
			}
			lineLen = len(line)
			os.Stdout.WriteString(line)
		}
	}

	doneChan <- testOutput
}

func match(oneOfPatternIfAny []string, noneOfPattern []string, candidate string) (matched bool, err error) {
	matched = len(oneOfPatternIfAny) == 0

	var didMatch bool
	for _, pattern := range oneOfPatternIfAny {
		didMatch, err = filepath.Match(pattern, candidate)
		if err != nil {
			return false, err
		}

		matched = didMatch || matched
	}

	for _, pattern := range noneOfPattern {
		didMatch, err = filepath.Match(pattern, candidate)
		if err != nil {
			return false, err
		}

		matched = !didMatch && matched
	}

	return matched, nil
}

func checkTests() {
	for _, test := range testCases {
		if !test.shouldFail && (len(test.expectedError) > 0 || len(test.expectedLocalError) > 0) {
			panic("Error expected without shouldFail in " + test.name)
		}

		if test.expectResumeRejected && !test.resumeSession {
			panic("expectResumeRejected without resumeSession in " + test.name)
		}

		if !test.skipVersionNameCheck {
			for _, ver := range tlsVersions {
				if !strings.Contains("-"+test.name+"-", "-"+ver.name+"-") {
					continue
				}

				found := test.config.MaxVersion == ver.version || test.config.MinVersion == ver.version || test.expectations.version == ver.version
				if test.resumeConfig != nil {
					found = found || test.resumeConfig.MaxVersion == ver.version || test.resumeConfig.MinVersion == ver.version
				}
				if test.resumeExpectations != nil {
					found = found || test.resumeExpectations.version == ver.version
				}
				shimFlag := ver.shimFlag(test.protocol)
				for _, flag := range test.flags {
					if flag == shimFlag {
						found = true
						break
					}
				}
				if !found {
					panic(fmt.Sprintf("The name of test %q suggests that it's version specific, but the test does not reference %s", test.name, ver.name))
				}
			}
		}

		for _, protocol := range []protocol{tls, dtls, quic} {
			if strings.Contains("-"+test.name+"-", "-"+protocol.String()+"-") && test.protocol != protocol {
				panic(fmt.Sprintf("The name of test %q suggests that it tests %q, but the test does not reference it", test.name, protocol))
			}
		}
	}
}

func main() {
	flag.Parse()
	var err error
	if tmpDir, err = os.MkdirTemp("", "testing-certs"); err != nil {
		fmt.Fprintf(os.Stderr, "failed to make temporary directory: %s", err)
		os.Exit(1)
	}
	defer os.RemoveAll(tmpDir)
	initKeys()
	initCertificates()

	if len(*shimConfigFile) != 0 {
		encoded, err := os.ReadFile(*shimConfigFile)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Couldn't read config file %q: %s\n", *shimConfigFile, err)
			os.Exit(1)
		}

		if err := json.Unmarshal(encoded, &shimConfig); err != nil {
			fmt.Fprintf(os.Stderr, "Couldn't decode config file %q: %s\n", *shimConfigFile, err)
			os.Exit(1)
		}
	}

	if shimConfig.AllCurves == nil {
		for _, curve := range testCurves {
			shimConfig.AllCurves = append(shimConfig.AllCurves, int(curve.id))
		}
	}

	addBasicTests()
	addCipherSuiteTests()
	addBadECDSASignatureTests()
	addCBCPaddingTests()
	addCBCSplittingTests()
	addClientAuthTests()
	addDDoSCallbackTests()
	addVersionNegotiationTests()
	addMinimumVersionTests()
	addExtensionTests()
	addResumptionVersionTests()
	addExtendedMasterSecretTests()
	addRenegotiationTests()
	addDTLSReplayTests()
	addSignatureAlgorithmTests()
	addDTLSRetransmitTests()
	addDTLSReorderTests()
	addExportKeyingMaterialTests()
	addExportTrafficSecretsTests()
	addTLSUniqueTests()
	addUnknownExtensionTests()
	addRSAClientKeyExchangeTests()
	addCurveTests()
	addSessionTicketTests()
	addTLS13RecordTests()
	addAllStateMachineCoverageTests()
	addChangeCipherSpecTests()
	addEndOfFlightTests()
	addWrongMessageTypeTests()
	addTrailingMessageDataTests()
	addTLS13HandshakeTests()
	addTLS13CipherPreferenceTests()
	addPeekTests()
	addRecordVersionTests()
	addCertificateTests()
	addRetainOnlySHA256ClientCertTests()
	addECDSAKeyUsageTests()
	addRSAKeyUsageTests()
	addExtraHandshakeTests()
	addOmitExtensionsTests()
	addCertCompressionTests()
	addJDK11WorkaroundTests()
	addDelegatedCredentialTests()
	addEncryptedClientHelloTests()
	addHintMismatchTests()
	addCompliancePolicyTests()
	addCertificateSelectionTests()
	addKeyUpdateTests()
	addPAKETests()
	addTrustAnchorTests()

	toAppend, err := convertToSplitHandshakeTests(testCases)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error making split handshake tests: %s", err)
		os.Exit(1)
	}
	testCases = append(testCases, toAppend...)

	checkTests()

	dispatcher, err := newShimDispatcher()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error opening socket: %s", err)
		os.Exit(1)
	}
	defer dispatcher.Close()

	numWorkers := *numWorkersFlag
	if useDebugger() {
		numWorkers = 1
	}

	statusChan := make(chan statusMsg, numWorkers)
	testChan := make(chan *testCase, numWorkers)
	doneChan := make(chan *testresult.Results)

	go statusPrinter(doneChan, statusChan, len(testCases))

	var wg sync.WaitGroup
	for i := 0; i < numWorkers; i++ {
		wg.Add(1)
		go worker(dispatcher, statusChan, testChan, *shimPath, &wg)
	}

	var oneOfPatternIfAny, noneOfPattern []string
	if len(*testToRun) > 0 {
		oneOfPatternIfAny = strings.Split(*testToRun, ";")
	}
	if len(*skipTest) > 0 {
		noneOfPattern = strings.Split(*skipTest, ";")
	}

	shardIndex, shardTotal, err := getSharding()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	if shardTotal > 0 {
		fmt.Printf("This is shard %d of 0..%d (inclusive)\n", shardIndex, shardTotal-1)
	}

	var foundTest bool
	for i := range testCases {
		if shardTotal > 0 && i%shardTotal != shardIndex {
			continue
		}

		matched, err := match(oneOfPatternIfAny, noneOfPattern, testCases[i].name)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error matching pattern: %s\n", err)
			os.Exit(1)
		}

		if !*includeDisabled {
			for pattern := range shimConfig.DisabledTests {
				isDisabled, err := filepath.Match(pattern, testCases[i].name)
				if err != nil {
					fmt.Fprintf(os.Stderr, "Error matching pattern %q from config file: %s\n", pattern, err)
					os.Exit(1)
				}

				if isDisabled {
					matched = false
					break
				}
			}
		}

		if matched {
			if foundTest && *useRR {
				fmt.Fprintf(os.Stderr, "Too many matching tests. Only one test can run when RR is enabled.\n")
				os.Exit(1)
			}

			foundTest = true
			testChan <- &testCases[i]

			if *repeatUntilFailure {
				break
			}
		}
	}

	if !foundTest && shardTotal == 0 {
		fmt.Fprintf(os.Stderr, "No tests run\n")
		os.Exit(1)
	}

	close(testChan)
	wg.Wait()
	close(statusChan)
	testOutput := <-doneChan

	fmt.Printf("\n")

	if *jsonOutput != "" {
		if err := testOutput.WriteToFile(*jsonOutput); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %s\n", err)
		}
	}

	if *useRR {
		fmt.Println("RR trace recorded. Replay with `rr replay`.")
	}

	if !testOutput.HasUnexpectedResults() {
		os.Exit(1)
	}
}
