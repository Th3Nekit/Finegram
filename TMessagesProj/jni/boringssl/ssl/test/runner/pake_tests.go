// Copyright 2025 The BoringSSL Authors
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

import "errors"

func addPAKETests() {
	spakeCredential := Credential{
		Type:         CredentialTypeSPAKE2PlusV1,
		PAKEContext:  []byte("context"),
		PAKEClientID: []byte("client"),
		PAKEServerID: []byte("server"),
		PAKEPassword: []byte("password"),
	}

	spakeWrongClientID := spakeCredential
	spakeWrongClientID.PAKEClientID = []byte("wrong")

	spakeWrongServerID := spakeCredential
	spakeWrongServerID.PAKEServerID = []byte("wrong")

	spakeWrongPassword := spakeCredential
	spakeWrongPassword.PAKEPassword = []byte("wrong")

	spakeWrongRole := spakeCredential
	spakeWrongRole.WrongPAKERole = true

	spakeWrongCodepoint := spakeCredential
	spakeWrongCodepoint.OverridePAKECodepoint = 1234

	testCases = append(testCases, testCase{
		name:     "PAKE-No-Server-Support",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
		},
		shouldFail:    true,
		expectedError: ":MISSING_KEY_SHARE:",
	})
	testCases = append(testCases, testCase{
		name:     "PAKE-Server",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{

				ExpectNoNewSessionTicket: true,
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-WrongClientID",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeWrongClientID,
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":PEER_PAKE_MISMATCH:",
		expectedLocalError: "remote error: handshake failure",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-WrongServerID",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeWrongServerID,
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":PEER_PAKE_MISMATCH:",
		expectedLocalError: "remote error: handshake failure",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-WrongCodepoint",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeWrongCodepoint,
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":PEER_PAKE_MISMATCH:",
		expectedLocalError: "remote error: handshake failure",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-MultiplePAKEs",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				OfferExtraPAKEs: []uint16{1, 2, 3, 4, 5},
			},
		},
		shimCredentials: []*Credential{&spakeWrongClientID, &spakeWrongServerID, &spakeWrongRole, &spakeCredential, &rsaCertificate},
		flags:           []string{"-expect-selected-credential", "3"},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-CertificateBeforePAKE",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				OfferExtraPAKEClientID: spakeCredential.PAKEClientID,
				OfferExtraPAKEServerID: spakeCredential.PAKEServerID,
				OfferExtraPAKEs:        []uint16{spakeID},
			},
		},
		shimCredentials: []*Credential{&rsaCertificate, &spakeCredential},
		flags:           []string{"-expect-selected-credential", "0"},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-NormalClient",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":PEER_PAKE_MISMATCH:",
		expectedLocalError: "remote error: handshake failure",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-NormalTLS12Client",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS12,
			MaxVersion: VersionTLS12,
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":NO_SHARED_CIPHER:",
		expectedLocalError: "remote error: handshake failure",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-ServerWithCertsToo-NormalClient",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
		},
		shimCredentials: []*Credential{&spakeCredential, &rsaCertificate},
		flags:           []string{"-expect-selected-credential", "1"},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-ServerWithCertsToo-NormalTLS12Client",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS12,
			MaxVersion: VersionTLS12,
		},
		shimCredentials: []*Credential{&spakeCredential, &rsaCertificate},
		flags:           []string{"-expect-selected-credential", "1"},
	})
	testCases = append(testCases, testCase{
		name:     "PAKE-Client",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				CheckClientHello: func(c *clientHelloMsg) error {

					if c.hasKeyShares {
						return errors.New("unexpected key_share extension")
					}
					if len(c.supportedCurves) != 0 {
						return errors.New("unexpected supported_groups extension")
					}

					if len(c.signatureAlgorithms) != 0 {
						return errors.New("unexpected signature_algorithms extension")
					}

					if len(c.pskKEModes) != 0 {
						return errors.New("unexpected psk_key_exchange_modes extension")
					}
					return nil
				},
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-HRRCookie",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte("cookie"),
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-HRRKeyShare",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCurve: CurveX25519,
			},
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-NormalClient-PAKEInHRR",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				AlwaysSendHelloRetryRequest: true,
				SendPAKEInHelloRetryRequest: true,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-EmptyServerHello",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				MissingKeyShare: true,
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
		shouldFail:      true,
		expectedError:   ":MISSING_EXTENSION:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-KeyShareServerHello",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				SkipHelloRetryRequest: true,

				IgnorePeerCurvePreferences: true,
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
		shouldFail:      true,
		expectedError:   ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-TLS12ServerHello",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS12,
			MaxVersion: VersionTLS12,
		},
		shimCredentials: []*Credential{&spakeCredential},
		shouldFail:      true,
		expectedError:   ":UNSUPPORTED_PROTOCOL:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-NormalClient-UnsolicitedPAKEInServerHello",
		testType: clientTest,
		config: Config{
			Bugs: ProtocolBugs{
				UnsolicitedPAKE: spakeID,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-WrongPAKEInServerHello",
		testType: clientTest,
		config: Config{
			Bugs: ProtocolBugs{
				UnsolicitedPAKE: 1234,
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
		shouldFail:      true,
		expectedError:   ":DECODE_ERROR:",
	})
	testCases = append(testCases, testCase{
		name:     "PAKE-Extension-Duplicate",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				OfferExtraPAKEClientID: []byte("client"),
				OfferExtraPAKEServerID: []byte("server"),
				OfferExtraPAKEs:        []uint16{1234, 1234},
			},
		},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-WrongPassword",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeWrongPassword,
		},
		shimCredentials: []*Credential{&spakeCredential},
		shouldFail:      true,
		expectedError:   ":DECODE_ERROR:",
	})
	testCases = append(testCases, testCase{
		name:     "PAKE-Client-Truncate",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				TruncatePAKEMessage: true,
			},
		},
		shimCredentials: []*Credential{&spakeCredential},
		shouldFail:      true,
		expectedError:   ":DECODE_ERROR:",
	})
	testCases = append(testCases, testCase{
		name:     "PAKE-Server-Truncate",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				TruncatePAKEMessage: true,
			},
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: illegal parameter",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-UnexpectedCertificateRequest",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			ClientAuth: RequireAnyClientCert,
			Bugs: ProtocolBugs{
				AlwaysSendCertificateRequest: true,
			},
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-UnexpectedCertificate",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{
				AlwaysSendCertificate:    true,
				UseCertificateCredential: &rsaCertificate,

				IgnorePeerSignatureAlgorithmPreferences: true,
			},
		},
		shimCredentials:    []*Credential{&spakeCredential},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-DoNotRequestClientCertificate",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
		},
		shimCredentials: []*Credential{&spakeCredential, &rsaCertificate},
		flags:           []string{"-require-any-client-certificate"},
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-WrongRole",
		testType: clientTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
		},
		shimCredentials: []*Credential{&spakeWrongRole},
		shouldFail:      true,

		expectedLocalError: "tls: client not configured with PAKE",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Server-WrongRole",
		testType: serverTest,
		config: Config{
			MinVersion: VersionTLS13,
			Credential: &spakeCredential,
		},
		shimCredentials: []*Credential{&spakeWrongRole},
		shouldFail:      true,

		expectedError:      ":UNKNOWN_CERTIFICATE_TYPE:",
		expectedLocalError: "remote error: handshake failure",
	})
	testCases = append(testCases, testCase{

		name:            "PAKE-Client-MultiplePAKEs",
		testType:        clientTest,
		shimCredentials: []*Credential{&spakeCredential, &spakeWrongPassword},
		shouldFail:      true,
		expectedError:   ":UNSUPPORTED_CREDENTIAL_LIST:",
	})
	testCases = append(testCases, testCase{

		name:            "PAKE-Client-PAKEAndCertificate",
		testType:        clientTest,
		shimCredentials: []*Credential{&spakeCredential, &rsaCertificate},
		shouldFail:      true,
		expectedError:   ":UNSUPPORTED_CREDENTIAL_LIST:",
	})
	testCases = append(testCases, testCase{

		name:     "PAKE-Client-NoResume",
		testType: clientTest,

		config: Config{
			Credential: &rsaCertificate,
		},

		resumeSession: true,
		resumeConfig: &Config{
			Credential: &spakeCredential,
			Bugs: ProtocolBugs{

				ExpectNoTLS13PSK: true,

				AlwaysSelectPSKIdentity: true,
			},
		},
		resumeShimCredentials: []*Credential{&spakeCredential},
		shouldFail:            true,
		expectedError:         ":UNEXPECTED_EXTENSION:",
	})
}
