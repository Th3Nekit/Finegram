// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package runner

import (
	"container/list"
	"crypto"
	"crypto/ecdsa"
	"crypto/rand"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"io"
	"math/big"
	"os"
	"sync"
	"time"

	"boringssl.googlesource.com/boringssl.git/ssl/test/runner/hpke"
)

const (
	VersionSSL30 = 0x0300
	VersionTLS10 = 0x0301
	VersionTLS11 = 0x0302
	VersionTLS12 = 0x0303
	VersionTLS13 = 0x0304
)

const (
	VersionDTLS10 = 0xfeff
	VersionDTLS12 = 0xfefd
	VersionDTLS13 = 0xfefc
)

var allTLSWireVersions = []uint16{
	VersionTLS13,
	VersionTLS12,
	VersionTLS11,
	VersionTLS10,
	VersionSSL30,
}

var allDTLSWireVersions = []uint16{
	VersionDTLS13,
	VersionDTLS12,
	VersionDTLS10,
}

const (
	maxPlaintext           = 16384
	maxCiphertext          = 16384 + 2048
	tlsRecordHeaderLen     = 5
	dtlsMaxRecordHeaderLen = 13
	maxHandshake           = 65536

	minVersion = VersionSSL30
	maxVersion = VersionTLS13
)

type recordType uint8

const (
	recordTypeChangeCipherSpec   recordType = 20
	recordTypeAlert              recordType = 21
	recordTypeHandshake          recordType = 22
	recordTypeApplicationData    recordType = 23
	recordTypePlaintextHandshake recordType = 24
	recordTypeACK                recordType = 26
)

const (
	typeHelloRequest          uint8 = 0
	typeClientHello           uint8 = 1
	typeServerHello           uint8 = 2
	typeHelloVerifyRequest    uint8 = 3
	typeNewSessionTicket      uint8 = 4
	typeEndOfEarlyData        uint8 = 5
	typeEncryptedExtensions   uint8 = 8
	typeCertificate           uint8 = 11
	typeServerKeyExchange     uint8 = 12
	typeCertificateRequest    uint8 = 13
	typeServerHelloDone       uint8 = 14
	typeCertificateVerify     uint8 = 15
	typeClientKeyExchange     uint8 = 16
	typeFinished              uint8 = 20
	typeCertificateStatus     uint8 = 22
	typeKeyUpdate             uint8 = 24
	typeCompressedCertificate uint8 = 25
	typeNextProtocol          uint8 = 67
	typeChannelID             uint8 = 203
	typeMessageHash           uint8 = 254
)

func messageTypeToString(typ uint8) string {
	switch typ {
	case typeHelloRequest:
		return "HelloRequest"
	case typeClientHello:
		return "ClientHello"
	case typeServerHello:
		return "ServerHello"
	case typeHelloVerifyRequest:
		return "HelloVerifyRequest"
	case typeNewSessionTicket:
		return "NewSessionTicket"
	case typeEndOfEarlyData:
		return "EndOfEarlyData"
	case typeEncryptedExtensions:
		return "EncryptedExtensions"
	case typeCertificate:
		return "Certificate"
	case typeServerKeyExchange:
		return "ServerKeyExchange"
	case typeCertificateRequest:
		return "CertificateRequest"
	case typeServerHelloDone:
		return "ServerHelloDone"
	case typeCertificateVerify:
		return "CertificateVerify"
	case typeClientKeyExchange:
		return "ClientKeyExchange"
	case typeFinished:
		return "Finished"
	case typeCertificateStatus:
		return "CertificateStatus"
	case typeKeyUpdate:
		return "KeyUpdate"
	case typeCompressedCertificate:
		return "CompressedCertificate"
	case typeNextProtocol:
		return "NextProtocol"
	case typeChannelID:
		return "ChannelID"
	case typeMessageHash:
		return "MessageHash"
	}
	return fmt.Sprintf("unknown(%d)", typ)
}

const (
	compressionNone uint8 = 0
)

const (
	extensionServerName                 uint16 = 0
	extensionStatusRequest              uint16 = 5
	extensionSupportedCurves            uint16 = 10
	extensionSupportedPoints            uint16 = 11
	extensionSignatureAlgorithms        uint16 = 13
	extensionUseSRTP                    uint16 = 14
	extensionALPN                       uint16 = 16
	extensionSignedCertificateTimestamp uint16 = 18
	extensionPadding                    uint16 = 21
	extensionExtendedMasterSecret       uint16 = 23
	extensionCompressedCertAlgs         uint16 = 27
	extensionDelegatedCredential        uint16 = 34
	extensionSessionTicket              uint16 = 35
	extensionPreSharedKey               uint16 = 41
	extensionEarlyData                  uint16 = 42
	extensionSupportedVersions          uint16 = 43
	extensionCookie                     uint16 = 44
	extensionPSKKeyExchangeModes        uint16 = 45
	extensionCertificateAuthorities     uint16 = 47
	extensionSignatureAlgorithmsCert    uint16 = 50
	extensionKeyShare                   uint16 = 51
	extensionQUICTransportParams        uint16 = 57
	extensionTLSFlags                   uint16 = 62
	extensionCustom                     uint16 = 1234
	extensionNextProtoNeg               uint16 = 13172
	extensionApplicationSettingsOld     uint16 = 17513
	extensionApplicationSettings        uint16 = 17613
	extensionRenegotiationInfo          uint16 = 0xff01
	extensionQUICTransportParamsLegacy  uint16 = 0xffa5
	extensionChannelID                  uint16 = 30032
	extensionPAKE                       uint16 = 35387
	extensionTrustAnchors               uint16 = 0xca34
	extensionDuplicate                  uint16 = 0xffff
	extensionEncryptedClientHello       uint16 = 0xfe0d
	extensionECHOuterExtensions         uint16 = 0xfd00
)

const (
	flagResumptionAcrossNames = 8
)

const (
	scsvRenegotiation uint16 = 0x00ff
)

var tls13HelloRetryRequest = []uint8{
	0xcf, 0x21, 0xad, 0x74, 0xe5, 0x9a, 0x61, 0x11, 0xbe, 0x1d, 0x8c,
	0x02, 0x1e, 0x65, 0xb8, 0x91, 0xc2, 0xa2, 0x11, 0x16, 0x7a, 0xbb,
	0x8c, 0x5e, 0x07, 0x9e, 0x09, 0xe2, 0xc8, 0xa8, 0x33, 0x9c,
}

type CurveID uint16

const (
	CurveP224           CurveID = 21
	CurveP256           CurveID = 23
	CurveP384           CurveID = 24
	CurveP521           CurveID = 25
	CurveX25519         CurveID = 29
	CurveX25519MLKEM768 CurveID = 0x11ec
	CurveX25519Kyber768 CurveID = 0x6399
)

const (
	pointFormatUncompressed    uint8 = 0
	pointFormatCompressedPrime uint8 = 1
)

const (
	statusTypeOCSP uint8 = 1
)

const (
	CertTypeRSASign    = 1
	CertTypeDSSSign    = 2
	CertTypeRSAFixedDH = 3
	CertTypeDSSFixedDH = 4

	CertTypeECDSASign      = 64
	CertTypeRSAFixedECDH   = 65
	CertTypeECDSAFixedECDH = 66

)

type signatureAlgorithm uint16

const (

	signatureRSAPKCS1WithMD5    signatureAlgorithm = 0x0101
	signatureRSAPKCS1WithSHA1   signatureAlgorithm = 0x0201
	signatureRSAPKCS1WithSHA256 signatureAlgorithm = 0x0401
	signatureRSAPKCS1WithSHA384 signatureAlgorithm = 0x0501
	signatureRSAPKCS1WithSHA512 signatureAlgorithm = 0x0601

	signatureECDSAWithSHA1          signatureAlgorithm = 0x0203
	signatureECDSAWithP256AndSHA256 signatureAlgorithm = 0x0403
	signatureECDSAWithP384AndSHA384 signatureAlgorithm = 0x0503
	signatureECDSAWithP521AndSHA512 signatureAlgorithm = 0x0603

	signatureRSAPSSWithSHA256 signatureAlgorithm = 0x0804
	signatureRSAPSSWithSHA384 signatureAlgorithm = 0x0805
	signatureRSAPSSWithSHA512 signatureAlgorithm = 0x0806

	signatureEd25519 signatureAlgorithm = 0x0807
	signatureEd448   signatureAlgorithm = 0x0808

	signatureRSAPKCS1WithSHA256Legacy signatureAlgorithm = 0x0420

	signatureRSAPKCS1WithMD5AndSHA1 signatureAlgorithm = 0xff01
)

var supportedSignatureAlgorithms = []signatureAlgorithm{
	signatureRSAPSSWithSHA256,
	signatureRSAPSSWithSHA384,
	signatureRSAPKCS1WithSHA256,
	signatureECDSAWithP256AndSHA256,
	signatureECDSAWithP384AndSHA384,
	signatureRSAPKCS1WithSHA1,
	signatureRSAPKCS1WithSHA256,
	signatureRSAPKCS1WithSHA384,
	signatureECDSAWithSHA1,
	signatureEd25519,
}

const (
	SRTP_AES128_CM_HMAC_SHA1_80 uint16 = 0x0001
	SRTP_AES128_CM_HMAC_SHA1_32        = 0x0002
)

const (
	pskKEMode    = 0
	pskDHEKEMode = 1
)

const (
	keyUpdateNotRequested = 0
	keyUpdateRequested    = 1
)

const echAcceptConfirmationLength = 8

const spakeID uint16 = 0x7d96

type ConnectionState struct {
	Version                    uint16
	HandshakeComplete          bool
	DidResume                  bool
	CipherSuite                uint16
	NegotiatedProtocol         string
	NegotiatedProtocolIsMutual bool
	NegotiatedProtocolFromALPN bool
	ServerName                 string
	PeerCertificates           []*x509.Certificate
	PeerDelegatedCredential    []byte
	VerifiedChains             [][]*x509.Certificate
	OCSPResponse               []byte
	ChannelID                  *ecdsa.PublicKey
	SRTPProtectionProfile      uint16
	TLSUnique                  []byte
	SCTList                    []byte
	PeerSignatureAlgorithm     signatureAlgorithm
	CurveID                    CurveID
	QUICTransportParams        []byte
	QUICTransportParamsLegacy  []byte
	HasApplicationSettings     bool
	PeerApplicationSettings    []byte
	HasApplicationSettingsOld  bool
	PeerApplicationSettingsOld []byte
	ECHAccepted                bool
}

type ClientAuthType int

const (
	NoClientCert ClientAuthType = iota
	RequestClientCert
	RequireAnyClientCert
	VerifyClientCertIfGiven
	RequireAndVerifyClientCert
)

type ClientSessionState struct {
	sessionID                   []uint8
	sessionTicket               []uint8
	vers                        uint16
	wireVersion                 uint16
	cipherSuite                 *cipherSuite
	secret                      []byte
	handshakeHash               []byte
	serverCertificates          []*x509.Certificate
	serverDelegatedCredential   []byte
	extendedMasterSecret        bool
	sctList                     []byte
	ocspResponse                []byte
	earlyALPN                   string
	ticketCreationTime          time.Time
	ticketExpiration            time.Time
	ticketAgeAdd                uint32
	maxEarlyDataSize            uint32
	hasApplicationSettings      bool
	localApplicationSettings    []byte
	peerApplicationSettings     []byte
	hasApplicationSettingsOld   bool
	localApplicationSettingsOld []byte
	peerApplicationSettingsOld  []byte
	resumptionAcrossNames       bool
}

type ClientSessionCache interface {

	Get(sessionKey string) (session *ClientSessionState, ok bool)

	Put(sessionKey string, cs *ClientSessionState)
}

type ServerSessionCache interface {

	Get(sessionID string) (session *sessionState, ok bool)

	Put(sessionID string, session *sessionState)
}

type CertCompressionAlg struct {

	Compress func([]byte) []byte

	Decompress func(out, in []byte) bool
}

type QUICUseCodepoint int

const (
	QUICUseCodepointStandard QUICUseCodepoint = iota
	QUICUseCodepointLegacy
	QUICUseCodepointBoth
	QUICUseCodepointNeither
	NumQUICUseCodepoints
)

func (c QUICUseCodepoint) IncludeStandard() bool {
	return c == QUICUseCodepointStandard || c == QUICUseCodepointBoth
}

func (c QUICUseCodepoint) IncludeLegacy() bool {
	return c == QUICUseCodepointLegacy || c == QUICUseCodepointBoth
}

func (c QUICUseCodepoint) String() string {
	switch c {
	case QUICUseCodepointStandard:
		return "Standard"
	case QUICUseCodepointLegacy:
		return "Legacy"
	case QUICUseCodepointBoth:
		return "Both"
	case QUICUseCodepointNeither:
		return "Neither"
	}
	panic("unknown value")
}

type ALPSUseCodepoint int

const (
	ALPSUseCodepointNew ALPSUseCodepoint = iota
	ALPSUseCodepointOld
	NumALPSUseCodepoints
)

func (c ALPSUseCodepoint) IncludeNew() bool {
	return c == ALPSUseCodepointNew
}

func (c ALPSUseCodepoint) IncludeOld() bool {
	return c == ALPSUseCodepointOld
}

func (c ALPSUseCodepoint) String() string {
	switch c {
	case ALPSUseCodepointNew:
		return "New"
	case ALPSUseCodepointOld:
		return "Old"
	}
	panic("unknown value")
}

type Config struct {

	Rand io.Reader

	Time func() time.Time

	Credential *Credential

	RootCAs *x509.CertPool

	SendRootCAs bool

	NextProtos []string

	NoFallbackNextProto bool

	NegotiateNPNWithNoProtos bool

	ApplicationSettings map[string][]byte

	ALPSUseNewCodepoint ALPSUseCodepoint

	ServerName string

	ClientECHConfig *ECHConfig

	ECHCipherSuites []HPKECipherSuite

	ServerECHConfigs []ServerECHConfig

	ECHOuterExtensions []uint16

	ClientAuth ClientAuthType

	ClientCAs *x509.CertPool

	ClientCertificateTypes []byte

	InsecureSkipVerify bool

	CipherSuites []uint16

	PreferServerCipherSuites bool

	SessionTicketsDisabled bool

	SessionTicketKey [32]byte

	ClientSessionCache ClientSessionCache

	ServerSessionCache ServerSessionCache

	MinVersion uint16

	MaxVersion uint16

	CurvePreferences []CurveID

	DefaultCurves []CurveID

	ChannelID *ecdsa.PrivateKey

	RequestChannelID bool

	PreSharedKey []byte

	PreSharedKeyIdentity string

	MaxEarlyDataSize uint32

	SRTPProtectionProfiles []uint16

	VerifySignatureAlgorithms []signatureAlgorithm

	DelegatedCredentialAlgorithms []signatureAlgorithm

	QUICTransportParams []byte

	QUICTransportParamsUseLegacyCodepoint QUICUseCodepoint

	CertCompressionAlgs map[uint16]CertCompressionAlg

	DTLSUseShortSeqNums bool

	DTLSRecordHeaderOmitLength bool

	RequestTrustAnchors [][]byte

	AvailableTrustAnchors [][]byte

	ResumptionAcrossNames bool

	Bugs ProtocolBugs

	serverInitOnce sync.Once
}

type BadValue int

const (
	BadValueNone BadValue = iota
	BadValueNegative
	BadValueZero
	BadValueLimit
	BadValueLarge
	NumBadValues
)

type RSABadValue int

const (
	RSABadValueNone RSABadValue = iota
	RSABadValueCorrupt
	RSABadValueTooLong
	RSABadValueTooShort
	RSABadValueWrongVersion1
	RSABadValueWrongVersion2
	RSABadValueWrongBlockType
	RSABadValueWrongLeadingByte
	RSABadValueNoZero
	NumRSABadValues
)

type ProtocolBugs struct {

	InvalidSignature bool

	SendCurve CurveID

	ECDHPointNotOnCurve bool

	TruncateKeyShare bool

	PadKeyShare bool

	BadECDSAR BadValue
	BadECDSAS BadValue

	MaxPadding bool

	PaddingFirstByteBad bool

	PaddingFirstByteBadIf255 bool

	FailIfNotFallbackSCSV bool

	DuplicateExtension bool

	UnauthenticatedECDH bool

	SkipHelloVerifyRequest bool

	ForceHelloVerifyRequest bool

	HelloVerifyRequestCookieLength int

	EmptyHelloVerifyRequestCookie bool

	SkipCertificateStatus bool

	SkipServerKeyExchange bool

	SkipNewSessionTicket bool

	UseFirstSessionTicket bool

	SkipClientCertificate bool

	SkipChangeCipherSpec bool

	SkipFinished bool

	SkipEndOfEarlyData bool

	NonEmptyEndOfEarlyData bool

	SendEndOfEarlyDataInQUICAndDTLS bool

	SkipCertificateVerify bool

	EarlyChangeCipherSpec int

	FragmentAcrossChangeCipherSpec bool

	SendExtraChangeCipherSpec int

	SendPostHandshakeChangeCipherSpec bool

	PartialEncryptedExtensionsWithServerHello bool

	PartialClientFinishedWithClientHello bool

	PartialClientFinishedWithSecondClientHello bool

	PartialEndOfEarlyDataWithClientHello bool

	PartialSecondClientHelloAfterFirst bool

	PartialClientKeyExchangeWithClientHello bool

	PartialNewSessionTicketWithServerHelloDone bool

	PartialFinishedWithServerHelloDone bool

	PartialServerHelloWithHelloRetryRequest bool

	TrailingDataWithFinished bool

	SendV2ClientHello bool

	V2ClientHelloChallengeLength int

	SendFallbackSCSV bool

	SendRenegotiationSCSV bool

	MaxHandshakeRecordLength int

	FragmentAlert bool

	DoubleAlert bool

	SendSpuriousAlert alert

	BadRSAClientKeyExchange RSABadValue

	RenewTicketOnResume bool

	SendClientVersion uint16

	OmitSupportedVersions bool

	SendSupportedVersions []uint16

	NegotiateVersion uint16

	NegotiateVersionOnRenego uint16

	ExpectFalseStart bool

	AlertBeforeFalseStartTest alert

	ExpectServerName string

	ExpectOuterServerName string

	ExpectClientECH bool

	ExpectNoClientECH bool

	IgnoreECHConfigCipherPreferences bool

	ExpectECHRetryConfigs []byte

	SendECHRetryConfigs []byte

	AlwaysSendECHRetryConfigs bool

	AlwaysSendECHHelloRetryRequest bool

	SendInvalidECHInner []byte

	OmitECHInner bool

	OmitSecondECHInner bool

	OmitServerHelloECHConfirmation bool

	AlwaysSendECHInner bool

	TruncateClientECHEnc bool

	ClientECHPadding int

	BadClientECHPadding bool

	OfferSessionInClientHelloOuter bool

	OnlyCompressSecondClientHelloInner bool

	OmitSecondEncryptedClientHello bool

	CorruptEncryptedClientHello bool

	CorruptSecondEncryptedClientHello bool

	CorruptSecondEncryptedClientHelloConfigID bool

	AllowTLS12InClientHelloInner bool

	MinimalClientHelloOuter bool

	ExpectECHOuterExtensions []uint16

	ExpectECHUncompressedExtensions []uint16

	ECHOuterExtensionOrder []uint16

	UseInnerSessionWithClientHelloOuter bool

	RecordClientHelloInner func(encodedInner, outer []byte) error

	SwapNPNAndALPN bool

	ALPNProtocol *string

	AlwaysNegotiateApplicationSettingsBoth bool

	AlwaysNegotiateApplicationSettingsNew bool

	AlwaysNegotiateApplicationSettingsOld bool

	SendApplicationSettingsWithEarlyData bool

	AlwaysSendClientEncryptedExtensions bool

	OmitClientEncryptedExtensions bool

	OmitClientApplicationSettings bool

	SendExtraClientEncryptedExtension bool

	AcceptAnySession bool

	SendBothTickets bool

	FilterTicket func([]byte) ([]byte, error)

	TicketSessionIDLength int

	EmptyTicketSessionID bool

	NewSessionIDLength int

	SendClientHelloSessionID []byte

	ExpectClientHelloSessionID bool

	EchoSessionIDInFullHandshake bool

	ExpectNoSessionID bool

	ExpectNoTLS12Session bool

	ExpectNoTLS12TicketSupport bool

	ExpectNoTLS13PSK bool

	ExpectNoTLS13PSKAfterHRR bool

	RequireExtendedMasterSecret bool

	NoExtendedMasterSecret bool

	NoExtendedMasterSecretOnRenegotiation bool

	EmptyRenegotiationInfo bool

	BadRenegotiationInfo bool

	BadRenegotiationInfoEnd bool

	NoRenegotiationInfo bool

	NoRenegotiationInfoInInitial bool

	NoRenegotiationInfoAfterInitial bool

	RequireRenegotiationInfo bool

	SequenceNumberMapping func(uint64) uint64

	RSAEphemeralKey bool

	SRTPMasterKeyIdentifier string

	SendSRTPProtectionProfile uint16

	NoSignatureAlgorithms bool

	NoSupportedCurves bool

	RequireSameRenegoClientVersion bool

	ExpectInitialRecordVersion uint16

	SendRecordVersion uint16

	SendInitialRecordVersion uint16

	MaxPacketLength int

	SendCipherSuite uint16

	SendCipherSuites []uint16

	AppDataBeforeHandshake []byte

	AppDataAfterChangeCipherSpec []byte

	AlertAfterChangeCipherSpec alert

	AppDataBeforeTLS13KeyChange []byte

	UnencryptedEncryptedExtensions bool

	PacketAdaptor *packetAdaptor

	WriteFlightDTLS WriteFlightFunc

	ACKFlightDTLS ACKFlightFunc

	SkipImplicitACKRead bool

	MockQUICTransport *mockQUICTransport

	ReorderHandshakeFragments bool

	MixCompleteMessageWithFragments bool

	SendInvalidRecordType bool

	SendWrongMessageType byte

	SendTrailingMessageData byte

	SplitFragments int

	SendEmptyFragments bool

	SendSplitAlert bool

	FailIfResumeOnRenego bool

	IgnorePeerCipherPreferences bool

	IgnorePeerSignatureAlgorithmPreferences bool

	IgnorePeerCurvePreferences bool

	BadFinished bool

	PackHandshakeFragments int

	PackHandshakeRecords int

	PackAppDataWithHandshake bool

	SplitAndPackAppData bool

	PackHandshakeFlight bool

	AdvertiseAllConfiguredCiphers bool

	EmptyCertificateList bool

	ExpectNewTicket bool

	RequireClientHelloSize int

	CustomExtension string

	CustomUnencryptedExtension string

	ExpectedCustomExtension *string

	CustomTicketExtension string

	CustomHelloRetryRequestExtension string

	NoCloseNotify bool

	SendAlertOnShutdown alert

	ExpectCloseNotify bool

	SendLargeRecords bool

	NegotiateALPNAndNPN bool

	SendALPN string

	SendUnencryptedALPN string

	SendEmptySessionTicket bool

	SendPSKKeyExchangeModes []byte

	ExpectNoNewSessionTicket bool

	ExpectNoNonEmptyNewSessionTicket bool

	DuplicateTicketEarlyData bool

	ExpectTicketEarlyData bool

	ExpectTicketAge time.Duration

	SendTicketAge time.Duration

	SendHelloRequestBeforeEveryAppDataRecord bool

	SendHelloRequestBeforeEveryHandshakeMessage bool

	BadChangeCipherSpec []byte

	BadHelloRequest []byte

	RequireSessionTickets bool

	RequireSessionIDs bool

	NullAllCiphers bool

	SendSCTListOnResume []byte

	SendSCTListOnRenegotiation []byte

	SendOCSPResponseOnResume []byte

	SendOCSPResponseOnRenegotiation []byte

	SendExtensionOnCertificate []byte

	SendOCSPOnIntermediates []byte

	SendSCTOnIntermediates []byte

	SendDuplicateCertExtensions bool

	ExpectNoExtensionsOnIntermediate bool

	RecordPadding int

	OmitRecordContents bool

	OuterRecordType recordType

	SendSignatureAlgorithm signatureAlgorithm

	SkipECDSACurveCheck bool

	IgnoreSignatureVersionChecks bool

	NegotiateRenegotiationInfoAtAllVersions bool

	NegotiateNPNAtAllVersions bool

	NegotiateEMSAtAllVersions bool

	AdvertiseTicketExtension bool

	NegotiatePSKResumption bool

	AlwaysSelectPSKIdentity bool

	SelectPSKIdentityOnResume uint16

	ExtraPSKIdentity bool

	MissingKeyShare bool

	SecondClientHelloMissingKeyShare bool

	MisinterpretHelloRetryRequestCurve CurveID

	DuplicateKeyShares bool

	SendEarlyAlert bool

	SendFakeEarlyDataLength int

	SendStrayEarlyHandshake bool

	OmitEarlyDataExtension bool

	SendEarlyDataOnSecondClientHello bool

	InterleaveEarlyData bool

	SendEarlyData [][]byte

	ExpectEarlyDataAccepted bool

	AlwaysAcceptEarlyData bool

	AlwaysRejectEarlyData bool

	SendEarlyDataExtension bool

	ExpectEarlyData [][]byte

	ExpectLateEarlyData [][]byte

	ExpectHalfRTTData [][]byte

	EmptyEncryptedExtensions bool

	EncryptedExtensionsWithKeyShare bool

	AlwaysSendHelloRetryRequest bool

	SecondHelloRetryRequest bool

	SendHelloRetryRequestCurve CurveID

	SendHelloRetryRequestCipherSuite uint16

	SendHelloRetryRequestCookie []byte

	DuplicateHelloRetryRequestExtensions bool

	SendServerHelloVersion uint16

	SendServerSupportedVersionExtension uint16

	OmitServerSupportedVersionExtension bool

	SkipHelloRetryRequest bool

	PackHelloRequestWithFinished bool

	ExpectMissingKeyShare bool

	SendExtraFinished bool

	SendRequestContext []byte

	OmitCertificateRequestAlgorithms bool

	SendCustomCertificateRequest uint16

	AlwaysSendCertificateRequest bool

	AlwaysSendCertificate bool

	UseCertificateCredential *Credential

	SendSNIWarningAlert bool

	SendCompressionMethods []byte

	SendCompressionMethod byte

	AlwaysSendPreSharedKeyIdentityHint bool

	TrailingKeyShareData bool

	InvalidChannelIDSignature bool

	AlwaysNegotiateChannelID bool

	ExpectGREASE bool

	OmitPSKsOnSecondClientHello bool

	OnlyCorruptSecondPSKBinder bool

	SendShortPSKBinder bool

	SendInvalidPSKBinder bool

	SendNoPSKBinder bool

	SendExtraPSKBinder bool

	PSKBinderFirst bool

	NoOCSPStapling bool

	NoSignedCertificateTimestamps bool

	ExpectPeerRequestedTrustAnchors [][]byte

	ExpectPeerAvailableTrustAnchors [][]byte

	ExpectPeerMatchTrustAnchor *bool

	AlwaysMatchTrustAnchorID bool

	SendTrustAnchorWrongCertificate bool

	SendNonEmptyTrustAnchorMatch bool

	AlwaysSendAvailableTrustAnchors bool

	SendSupportedPointFormats []byte

	SendServerSupportedCurves bool

	MaxReceivePlaintext int

	ExpectPackedEncryptedHandshake int

	SendTicketLifetime time.Duration

	SendServerNameAck bool

	ExpectCertificateReqNames [][]byte

	RenegotiationCertificate *Credential

	SigningAlgorithmForLegacyVersions signatureAlgorithm

	AlwaysSignAsLegacyVersion bool

	RejectUnsolicitedKeyUpdate bool

	OmitExtensions bool

	EmptyExtensions bool

	ExpectOmitExtensions bool

	ExpectRecordSplitting bool

	PadClientHello int

	SendTLS13DowngradeRandom bool

	IgnoreTLS13DowngradeRandom bool

	SendCompressedCoordinates bool

	SetX25519HighBit bool

	LowOrderX25519Point bool

	MLKEMEncapKeyNotReduced bool

	DuplicateCompressedCertAlgs bool

	ExpectedCompressedCert uint16

	ExpectUncompressedCert bool

	SendCertCompressionAlgID uint16

	SendCertUncompressedLength uint32

	SendClientHelloWithFixes []byte

	SendJDK11DowngradeRandom bool

	ExpectJDK11DowngradeRandom bool

	FailIfHelloRetryRequested bool

	FailIfPostQuantumOffered bool

	ExpectedKeyShares []CurveID

	CompatModeWithQUIC bool

	DTLS13EchoSessionID bool

	DTLSUsePlaintextRecordHeader bool

	DTLS13RecordHeaderSetCIDBit bool

	EncryptSessionTicketKey *[32]byte

	OmitPublicName bool

	AllowEpochOverflow bool

	SendPAKEInHelloRetryRequest bool

	UnsolicitedPAKE uint16

	OfferExtraPAKEs []uint16

	OfferExtraPAKEClientID []byte
	OfferExtraPAKEServerID []byte

	TruncatePAKEMessage bool

	CheckClientHello func(*clientHelloMsg) error

	SendTicketFlags []uint

	AlwaysSendTicketFlags bool

	TicketFlagPadding int

	ExpectResumptionAcrossNames *bool
}

func (c *Config) serverInit() {
	if c.SessionTicketsDisabled {
		return
	}

	for _, b := range c.SessionTicketKey {
		if b != 0 {
			return
		}
	}

	if _, err := io.ReadFull(c.rand(), c.SessionTicketKey[:]); err != nil {
		c.SessionTicketsDisabled = true
	}
}

func (c *Config) rand() io.Reader {
	r := c.Rand
	if r == nil {
		return rand.Reader
	}
	return r
}

func (c *Config) time() time.Time {
	t := c.Time
	if t == nil {
		t = time.Now
	}
	return t()
}

func (c *Config) cipherSuites() []uint16 {
	s := c.CipherSuites
	if s == nil {
		s = defaultCipherSuites()
	}
	return s
}

func (c *Config) minVersion(isDTLS bool) uint16 {
	ret := uint16(minVersion)
	if c != nil && c.MinVersion != 0 {
		ret = c.MinVersion
	}
	if isDTLS {

		if ret < VersionTLS10 {
			return VersionTLS10
		}

		if ret == VersionTLS11 {
			return VersionTLS12
		}
	}
	return ret
}

func (c *Config) maxVersion(isDTLS bool) uint16 {
	ret := uint16(maxVersion)
	if c != nil && c.MaxVersion != 0 {
		ret = c.MaxVersion
	}
	if isDTLS {

		if ret == VersionTLS11 {
			return VersionTLS10
		}
	}
	return ret
}

var defaultCurvePreferences = []CurveID{CurveX25519MLKEM768, CurveX25519Kyber768, CurveX25519, CurveP256, CurveP384, CurveP521}

func (c *Config) curvePreferences() []CurveID {
	if c == nil || len(c.CurvePreferences) == 0 {
		return defaultCurvePreferences
	}
	return c.CurvePreferences
}

func (c *Config) defaultCurves() map[CurveID]bool {
	defaultCurves := make(map[CurveID]bool)
	curves := c.DefaultCurves
	if c == nil || c.DefaultCurves == nil {
		curves = c.curvePreferences()
	}
	for _, curveID := range curves {
		defaultCurves[curveID] = true
	}
	return defaultCurves
}

var defaultECHCipherSuitePreferences = []HPKECipherSuite{
	{KDF: hpke.HKDFSHA256, AEAD: hpke.AES128GCM},
	{KDF: hpke.HKDFSHA256, AEAD: hpke.AES256GCM},
	{KDF: hpke.HKDFSHA256, AEAD: hpke.ChaCha20Poly1305},
}

func (c *Config) echCipherSuitePreferences() []HPKECipherSuite {
	if c == nil || len(c.ECHCipherSuites) == 0 {
		return defaultECHCipherSuitePreferences
	}
	return c.ECHCipherSuites
}

func wireToVersion(vers uint16, isDTLS bool) (uint16, bool) {
	if isDTLS {
		switch vers {
		case VersionDTLS13:
			return VersionTLS13, true
		case VersionDTLS12:
			return VersionTLS12, true
		case VersionDTLS10:
			return VersionTLS10, true
		}
	} else {
		switch vers {
		case VersionSSL30, VersionTLS10, VersionTLS11, VersionTLS12, VersionTLS13:
			return vers, true
		}
	}

	return 0, false
}

func (c *Config) isSupportedVersion(wireVers uint16, isDTLS bool) (uint16, bool) {
	vers, ok := wireToVersion(wireVers, isDTLS)
	if !ok || c.minVersion(isDTLS) > vers || vers > c.maxVersion(isDTLS) {
		return 0, false
	}
	return vers, true
}

func (c *Config) supportedVersions(isDTLS, requireTLS13 bool) []uint16 {
	versions := allTLSWireVersions
	if isDTLS {
		versions = allDTLSWireVersions
	}
	var ret []uint16
	for _, wireVers := range versions {
		vers, ok := c.isSupportedVersion(wireVers, isDTLS)
		if !ok {
			continue
		}
		if requireTLS13 && vers < VersionTLS13 {
			continue
		}
		ret = append(ret, wireVers)
	}
	return ret
}

func (c *Config) verifySignatureAlgorithms() []signatureAlgorithm {
	if c != nil && c.VerifySignatureAlgorithms != nil {
		return c.VerifySignatureAlgorithms
	}
	return supportedSignatureAlgorithms
}

type CredentialType int

const (
	CredentialTypeX509 CredentialType = iota
	CredentialTypeDelegated
	CredentialTypeSPAKE2PlusV1
)

type Credential struct {
	Type CredentialType

	Certificate [][]byte

	RootCertificate []byte
	PrivateKey      crypto.PrivateKey

	OCSPStaple []byte

	SignedCertificateTimestampList []byte

	SignatureAlgorithms []signatureAlgorithm

	Leaf *x509.Certificate

	DelegatedCredential []byte

	ChainPath string

	KeyPath string

	RootPath string

	SignSignatureAlgorithms []signatureAlgorithm

	MustMatchIssuer bool

	PAKEContext  []byte
	PAKEClientID []byte
	PAKEServerID []byte
	PAKEPassword []byte

	WrongPAKERole bool

	OverridePAKECodepoint uint16

	TrustAnchorID []byte
}

func (c *Credential) WithSignatureAlgorithms(sigAlgs ...signatureAlgorithm) *Credential {
	ret := *c
	ret.SignatureAlgorithms = sigAlgs
	return &ret
}

func (c *Credential) WithOCSP(ocsp []byte) *Credential {
	ret := *c
	ret.OCSPStaple = ocsp
	return &ret
}

func (c *Credential) WithSCTList(sctList []byte) *Credential {
	ret := *c
	ret.SignedCertificateTimestampList = sctList
	return &ret
}

func (c *Credential) WithMustMatchIssuer(mustMatch bool) *Credential {
	ret := *c
	ret.MustMatchIssuer = mustMatch
	return &ret
}

func (c *Credential) signatureAlgorithms() []signatureAlgorithm {
	if c != nil && c.SignatureAlgorithms != nil {
		return c.SignatureAlgorithms
	}
	return supportedSignatureAlgorithms
}

func (c *Credential) WithTrustAnchorID(id []byte) *Credential {
	ret := *c
	ret.TrustAnchorID = id
	ret.MustMatchIssuer = true
	return &ret
}

type handshakeMessage interface {
	marshal() []byte
	unmarshal([]byte) bool
}

type lruSessionCache struct {
	sync.Mutex

	m        map[string]*list.Element
	q        *list.List
	capacity int
}

type lruSessionCacheEntry struct {
	sessionKey string
	state      any
}

func (c *lruSessionCache) Put(sessionKey string, cs any) {
	c.Lock()
	defer c.Unlock()

	if elem, ok := c.m[sessionKey]; ok {
		entry := elem.Value.(*lruSessionCacheEntry)
		entry.state = cs
		c.q.MoveToFront(elem)
		return
	}

	if c.q.Len() < c.capacity {
		entry := &lruSessionCacheEntry{sessionKey, cs}
		c.m[sessionKey] = c.q.PushFront(entry)
		return
	}

	elem := c.q.Back()
	entry := elem.Value.(*lruSessionCacheEntry)
	delete(c.m, entry.sessionKey)
	entry.sessionKey = sessionKey
	entry.state = cs
	c.q.MoveToFront(elem)
	c.m[sessionKey] = elem
}

func (c *lruSessionCache) Get(sessionKey string) (any, bool) {
	c.Lock()
	defer c.Unlock()

	if elem, ok := c.m[sessionKey]; ok {
		c.q.MoveToFront(elem)
		return elem.Value.(*lruSessionCacheEntry).state, true
	}
	return nil, false
}

type lruClientSessionCache struct {
	lruSessionCache
}

func (c *lruClientSessionCache) Put(sessionKey string, cs *ClientSessionState) {
	c.lruSessionCache.Put(sessionKey, cs)
}

func (c *lruClientSessionCache) Get(sessionKey string) (*ClientSessionState, bool) {
	cs, ok := c.lruSessionCache.Get(sessionKey)
	if !ok {
		return nil, false
	}
	return cs.(*ClientSessionState), true
}

type lruServerSessionCache struct {
	lruSessionCache
}

func (c *lruServerSessionCache) Put(sessionID string, session *sessionState) {
	c.lruSessionCache.Put(sessionID, session)
}

func (c *lruServerSessionCache) Get(sessionID string) (*sessionState, bool) {
	cs, ok := c.lruSessionCache.Get(sessionID)
	if !ok {
		return nil, false
	}
	return cs.(*sessionState), true
}

func NewLRUClientSessionCache(capacity int) ClientSessionCache {
	const defaultSessionCacheCapacity = 64

	if capacity < 1 {
		capacity = defaultSessionCacheCapacity
	}
	return &lruClientSessionCache{
		lruSessionCache{
			m:        make(map[string]*list.Element),
			q:        list.New(),
			capacity: capacity,
		},
	}
}

func NewLRUServerSessionCache(capacity int) ServerSessionCache {
	const defaultSessionCacheCapacity = 64

	if capacity < 1 {
		capacity = defaultSessionCacheCapacity
	}
	return &lruServerSessionCache{
		lruSessionCache{
			m:        make(map[string]*list.Element),
			q:        list.New(),
			capacity: capacity,
		},
	}
}

type dsaSignature struct {
	R, S *big.Int
}

type ecdsaSignature dsaSignature

var emptyConfig Config

func defaultConfig() *Config {
	return &emptyConfig
}

var (
	once                   sync.Once
	varDefaultCipherSuites []uint16
)

func defaultCipherSuites() []uint16 {
	once.Do(initDefaultCipherSuites)
	return varDefaultCipherSuites
}

func initDefaultCipherSuites() {
	for _, suite := range cipherSuites {
		if suite.flags&suitePSK == 0 {
			varDefaultCipherSuites = append(varDefaultCipherSuites, suite.id)
		}
	}
}

func unexpectedMessageError(wanted, got any) error {
	return fmt.Errorf("tls: received unexpected handshake message of type %T when waiting for %T", got, wanted)
}

var (

	downgradeTLS13 = []byte{0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x01}
	downgradeTLS12 = []byte{0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x00}

	downgradeJDK11 = []byte{0xed, 0xbf, 0xb4, 0xa8, 0xc2, 0x47, 0x10, 0xff}
)

func containsGREASE(values []uint16) bool {
	for _, v := range values {
		if isGREASEValue(v) {
			return true
		}
	}
	return false
}

func isAllZero(v []byte) bool {
	for _, b := range v {
		if b != 0 {
			return false
		}
	}
	return true
}

var baseCertTemplate = &x509.Certificate{
	SerialNumber: big.NewInt(57005),
	Subject: pkix.Name{
		CommonName:   "test cert",
		Country:      []string{"US"},
		Province:     []string{"Some-State"},
		Organization: []string{"Internet Widgits Pty Ltd"},
	},
	NotBefore:             time.Now().Add(-time.Hour),
	NotAfter:              time.Now().Add(time.Hour),
	DNSNames:              []string{"test"},
	IsCA:                  true,
	BasicConstraintsValid: true,
}

var tmpDir string

func generateSingleCertChain(template *x509.Certificate, key crypto.Signer) Credential {
	cert := generateTestCert(template, nil, key)
	tmpCertPath, tmpKeyPath := writeTempCertFile([]*x509.Certificate{cert}), writeTempKeyFile(key)
	return Credential{
		Certificate:     [][]byte{cert.Raw},
		RootCertificate: cert.Raw,
		PrivateKey:      key,
		Leaf:            cert,
		ChainPath:       tmpCertPath,
		KeyPath:         tmpKeyPath,
		RootPath:        tmpCertPath,
	}
}

func writeTempCertFile(certs []*x509.Certificate) string {
	f, err := os.CreateTemp(tmpDir, "test-cert")
	if err != nil {
		panic(fmt.Sprintf("failed to create temp file: %s", err))
	}
	for _, cert := range certs {
		if _, err := f.Write(pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: cert.Raw})); err != nil {
			panic(fmt.Sprintf("failed to write test certificate: %s", err))
		}
	}
	tmpCertPath := f.Name()
	if err := f.Close(); err != nil {
		panic(fmt.Sprintf("failed to close test certificate temp file: %s", err))
	}
	return tmpCertPath
}

func writeTempKeyFile(privKey crypto.Signer) string {
	f, err := os.CreateTemp(tmpDir, "test-key")
	if err != nil {
		panic(fmt.Sprintf("failed to create temp file: %s", err))
	}
	keyDER, err := x509.MarshalPKCS8PrivateKey(privKey)
	if err != nil {
		panic(fmt.Sprintf("failed to marshal test key: %s", err))
	}
	if _, err := f.Write(pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: keyDER})); err != nil {
		panic(fmt.Sprintf("failed to write test key: %s", err))
	}
	tmpKeyPath := f.Name()
	if err := f.Close(); err != nil {
		panic(fmt.Sprintf("failed to close test key temp file: %s", err))
	}
	return tmpKeyPath
}

func generateTestCert(template, issuer *x509.Certificate, key crypto.Signer) *x509.Certificate {
	if template == nil {
		template = baseCertTemplate
	}
	if issuer == nil {
		issuer = template
	}
	der, err := x509.CreateCertificate(rand.Reader, template, issuer, key.Public(), key)
	if err != nil {
		panic(fmt.Sprintf("failed to create test certificate: %s", err))
	}
	cert, err := x509.ParseCertificate(der)
	if err != nil {
		panic(fmt.Sprintf("failed to parse test certificate: %s", err))
	}

	return cert
}

func ptrTo[T any](t T) *T { return &t }
