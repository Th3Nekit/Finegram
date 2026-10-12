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

import "strconv"

func addTLS13RecordTests() {
	for _, protocol := range []protocol{tls, dtls} {
		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     "TLS13-RecordPadding-" + protocol.String(),
			config: Config{
				MaxVersion: VersionTLS13,
				MinVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					RecordPadding: 10,
				},
			},
		})

		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     "TLS13-EmptyRecords-" + protocol.String(),
			config: Config{
				MaxVersion: VersionTLS13,
				MinVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					OmitRecordContents: true,
				},
			},
			shouldFail:    true,
			expectedError: ":DECRYPTION_FAILED_OR_BAD_RECORD_MAC:",
		})

		testCases = append(testCases, testCase{
			protocol: protocol,
			name:     "TLS13-OnlyPadding-" + protocol.String(),
			config: Config{
				MaxVersion: VersionTLS13,
				MinVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					OmitRecordContents: true,
					RecordPadding:      10,
				},
			},
			shouldFail:    true,
			expectedError: ":DECRYPTION_FAILED_OR_BAD_RECORD_MAC:",
		})

		if protocol == tls {
			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "TLS13-WrongOuterRecord-" + protocol.String(),
				config: Config{
					MaxVersion: VersionTLS13,
					MinVersion: VersionTLS13,
					Bugs: ProtocolBugs{
						OuterRecordType: recordTypeHandshake,
					},
				},
				shouldFail:    true,
				expectedError: ":INVALID_OUTER_RECORD_TYPE:",
			})
		}
	}
}

func addTLS13HandshakeTests() {
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "NegotiatePSKResumption-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				NegotiatePSKResumption: true,
			},
		},
		resumeSession: true,
		shouldFail:    true,
		expectedError: ":MISSING_KEY_SHARE:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "MissingKeyShare-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				MissingKeyShare: true,
			},
		},
		shouldFail:    true,
		expectedError: ":MISSING_KEY_SHARE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "MissingKeyShare-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				MissingKeyShare: true,
			},
		},
		shouldFail:    true,
		expectedError: ":MISSING_KEY_SHARE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "DuplicateKeyShares-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				DuplicateKeyShares: true,
			},
		},
		shouldFail:    true,
		expectedError: ":DUPLICATE_KEY_SHARE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "ResumeTLS12SessionID-TLS13",
		config: Config{
			MaxVersion:             VersionTLS12,
			SessionTicketsDisabled: true,
		},
		flags:         []string{"-max-version", strconv.Itoa(VersionTLS13)},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "TLS12SessionID-TLS13",
		config: Config{
			MaxVersion:             VersionTLS12,
			SessionTicketsDisabled: true,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
		},
		resumeSession:        true,
		expectResumeRejected: true,
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EmptySessionID-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendClientHelloSessionID: []byte{},
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Server-ShortSessionID-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendClientHelloSessionID: make([]byte, 16),
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Server-FullSessionID-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendClientHelloSessionID: make([]byte, 32),
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Server-TooLongSessionID-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendClientHelloSessionID: make([]byte, 33),
			},
		},
		shouldFail:         true,
		expectedError:      ":CLIENTHELLO_PARSE_FAILED:",
		expectedLocalError: "remote error: error decoding message",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Server-TooLongSessionID-TLS12",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendClientHelloSessionID: make([]byte, 33),
			},
		},
		shouldFail:         true,
		expectedError:      ":CLIENTHELLO_PARSE_FAILED:",
		expectedLocalError: "remote error: error decoding message",
	})

	testCases = append(testCases, testCase{
		name: "Client-ShortSessionID",
		config: Config{
			MaxVersion:             VersionTLS12,
			SessionTicketsDisabled: true,
			Bugs: ProtocolBugs{
				NewSessionIDLength: 1,
			},
		},
		resumeSession: true,
	})
	testCases = append(testCases, testCase{
		name: "Client-TooLongSessionID",
		config: Config{
			MaxVersion:             VersionTLS12,
			SessionTicketsDisabled: true,
			Bugs: ProtocolBugs{
				NewSessionIDLength: 33,
			},
		},
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: error decoding message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "TLS13SessionID-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectClientHelloSessionID: true,
			},
		},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "TLS12NoSessionID-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectNoSessionID: true,
			},
		},
		flags: []string{"-max-version", strconv.Itoa(VersionTLS12)},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
		},
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-on-initial-expect-early-data-reason", "no_session_offered",
			"-on-resume-expect-early-data-reason", "accept",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-Reject-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysRejectEarlyData: true,
			},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-retry-expect-early-data-reason", "peer_declined",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
		},
		messageCount:  2,
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-on-initial-expect-early-data-reason", "no_session_offered",
			"-on-resume-expect-early-data-reason", "accept",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-FirstTicket-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				UseFirstSessionTicket: true,
			},
		},
		messageCount:  2,
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-on-resume-expect-early-data-reason", "accept",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-OmitEarlyDataExtension-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
				OmitEarlyDataExtension:  true,
			},
		},
		shouldFail:    true,
		expectedError: ":DECRYPTION_FAILED_OR_BAD_RECORD_MAC:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-OmitEarlyDataExtension-HelloRetryRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			DefaultCurves: []CurveID{},
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
				OmitEarlyDataExtension:  true,
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_RECORD:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-TooMuchData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 16384 + 1,
			},
		},
		shouldFail:    true,
		expectedError: ":TOO_MUCH_SKIPPED_EARLY_DATA:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-Interleaved-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
				InterleaveEarlyData:     true,
			},
		},
		shouldFail:    true,
		expectedError: ":DECRYPTION_FAILED_OR_BAD_RECORD_MAC:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-EarlyDataInTLS12-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
		flags:         []string{"-max-version", strconv.Itoa(VersionTLS12)},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-HRR-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
			},
			DefaultCurves: []CurveID{},
		},

		flags: []string{"-expect-early-data-reason", "disabled"},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-HRR-Interleaved-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 4,
				InterleaveEarlyData:     true,
			},
			DefaultCurves: []CurveID{},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-HRR-TooMuchData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendFakeEarlyDataLength: 16384 + 1,
			},
			DefaultCurves: []CurveID{},
		},
		shouldFail:    true,
		expectedError: ":TOO_MUCH_SKIPPED_EARLY_DATA:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-HRR-FatalAlert-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendEarlyAlert:          true,
				SendFakeEarlyDataLength: 4,
			},
			DefaultCurves: []CurveID{},
		},
		shouldFail:    true,
		expectedError: ":SSLV3_ALERT_HANDSHAKE_FAILURE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipEarlyData-SecondClientHelloEarlyData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendEarlyDataOnSecondClientHello: true,
			},
			DefaultCurves: []CurveID{},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: bad record MAC",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EmptyEncryptedExtensions-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				EmptyEncryptedExtensions: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: error decoding message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EncryptedExtensionsWithKeyShare-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				EncryptedExtensionsWithKeyShare: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SendHelloRetryRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			DefaultCurves:    []CurveID{},
			CurvePreferences: []CurveID{CurveX25519},
		},
		expectations: connectionExpectations{
			curveID: CurveX25519,
		},
		flags: []string{"-expect-hrr"},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SendHelloRetryRequest-2-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			DefaultCurves:    []CurveID{CurveP384},
			CurvePreferences: []CurveID{CurveX25519, CurveP384},
		},

		expectations: connectionExpectations{
			curveID: CurveX25519,
		},
		flags: []string{"-expect-hrr"},
	})

	testCases = append(testCases, testCase{
		name: "UnknownCurve-HelloRetryRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCurve: bogusCurve,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-CipherChange-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SendCipherSuite:                  TLS_AES_128_GCM_SHA256,
				SendHelloRetryRequestCipherSuite: TLS_CHACHA20_POLY1305_SHA256,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CIPHER_RETURNED:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "HelloRetryRequest-NonResumableCipher-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			CipherSuites: []uint16{
				TLS_AES_128_GCM_SHA256,
			},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				ExpectNoTLS13PSKAfterHRR: true,
			},
			CipherSuites: []uint16{
				TLS_AES_256_GCM_SHA384,
			},
		},
		resumeSession:        true,
		expectResumeRejected: true,
	})

	testCases = append(testCases, testCase{
		name: "DisabledCurve-HelloRetryRequest-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveP256},
			Bugs: ProtocolBugs{
				IgnorePeerCurvePreferences: true,
			},
		},
		flags:         []string{"-curves", strconv.Itoa(int(CurveP384))},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "UnnecessaryHelloRetryRequest-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveX25519},
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCurve: CurveX25519,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "SecondHelloRetryRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SecondHelloRetryRequest: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-Empty-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysSendHelloRetryRequest: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":EMPTY_HELLO_RETRY_REQUEST:",
		expectedLocalError: "remote error: illegal parameter",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-DuplicateCurve-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				ExpectMissingKeyShare:                true,
				DuplicateHelloRetryRequestExtensions: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":DUPLICATE_EXTENSION:",
		expectedLocalError: "remote error: illegal parameter",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-Cookie-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte("cookie"),
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-DuplicateCookie-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie:          []byte("cookie"),
				DuplicateHelloRetryRequestExtensions: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":DUPLICATE_EXTENSION:",
		expectedLocalError: "remote error: illegal parameter",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-EmptyCookie-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte{},
			},
		},
		shouldFail:    true,
		expectedError: ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-Cookie-Curve-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte("cookie"),
				ExpectMissingKeyShare:       true,
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequest-Unknown-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				CustomHelloRetryRequestExtension: "extension",
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SecondClientHelloMissingKeyShare-TLS13",
		config: Config{
			MaxVersion:    VersionTLS13,
			DefaultCurves: []CurveID{},
			Bugs: ProtocolBugs{
				SecondClientHelloMissingKeyShare: true,
			},
		},
		shouldFail:    true,
		expectedError: ":MISSING_KEY_SHARE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SecondClientHelloWrongCurve-TLS13",
		config: Config{
			MaxVersion:    VersionTLS13,
			DefaultCurves: []CurveID{},
			Bugs: ProtocolBugs{
				MisinterpretHelloRetryRequestCurve: CurveP521,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequestVersionMismatch-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SendServerHelloVersion: 0x0305,
			},
		},
		shouldFail:    true,
		expectedError: ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		name: "HelloRetryRequestCurveMismatch-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{

				SendHelloRetryRequestCurve: CurveP384,

				SendCurve: CurveP256,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "SkipHelloRetryRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SkipHelloRetryRequest: true,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "SecondServerHelloNoVersion-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				OmitServerSupportedVersionExtension: true,
			},
		},
		shouldFail:    true,
		expectedError: ":SECOND_SERVERHELLO_VERSION_MISMATCH:",
	})
	testCases = append(testCases, testCase{
		name: "SecondServerHelloWrongVersion-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SendServerSupportedVersionExtension: 0x1234,
			},
		},
		shouldFail:    true,
		expectedError: ":SECOND_SERVERHELLO_VERSION_MISMATCH:",
	})

	testCases = append(testCases, testCase{
		name: "RequestContextInHandshake-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
			Bugs: ProtocolBugs{
				SendRequestContext: []byte("request context"),
			},
		},
		shimCertificate: &rsaCertificate,
		shouldFail:      true,
		expectedError:   ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		name: "UnknownExtensionInCertificateRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
			Bugs: ProtocolBugs{
				SendCustomCertificateRequest: 0x1212,
			},
		},
		shimCertificate: &rsaCertificate,
	})

	testCases = append(testCases, testCase{
		name: "MissingSignatureAlgorithmsInCertificateRequest-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
			Bugs: ProtocolBugs{
				OmitCertificateRequestAlgorithms: true,
			},
		},
		shimCertificate: &rsaCertificate,
		shouldFail:      true,
		expectedError:   ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrailingKeyShareData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				TrailingKeyShareData: true,
			},
		},
		shouldFail:    true,
		expectedError: ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		name: "AlwaysSelectPSKIdentity-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysSelectPSKIdentity: true,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "InvalidPSKIdentity-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SelectPSKIdentityOnResume: 1,
			},
		},
		resumeSession: true,
		shouldFail:    true,
		expectedError: ":PSK_IDENTITY_NOT_FOUND:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ExtraPSKIdentity-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExtraPSKIdentity:   true,
				SendExtraPSKBinder: true,
			},
		},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		name: "CustomTicketExtension-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				CustomTicketExtension: "1234",
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-RejectTicket-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: &rsaCertificate,
		},
		resumeConfig: &Config{
			MaxVersion:             VersionTLS13,
			Credential:             &ecdsaP256Certificate,
			SessionTicketsDisabled: true,
		},
		resumeSession:           true,
		expectResumeRejected:    true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-retry-expect-early-data-reason", "session_not_resumed",

			"-on-initial-expect-peer-cert-file", rsaCertificate.ChainPath,
			"-on-resume-expect-peer-cert-file", rsaCertificate.ChainPath,
			"-on-retry-expect-peer-cert-file", ecdsaP256Certificate.ChainPath,

			"-on-retry-expect-no-session",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-RejectTicket-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				FilterTicket: func(in []byte) ([]byte, error) {
					in[len(in)-1] ^= 1
					return in, nil
				},
			},
		},
		messageCount:            2,
		resumeSession:           true,
		expectResumeRejected:    true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-resume-expect-early-data-reason", "session_not_resumed",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-HRR-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte{1, 2, 3, 4},
			},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-retry-expect-early-data-reason", "hello_retry_request",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-HRR-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,

			DefaultCurves: []CurveID{},
		},
		messageCount:            2,
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-resume-expect-early-data-reason", "hello_retry_request",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-HRR-RejectTicket-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: &rsaCertificate,
		},
		resumeConfig: &Config{
			MaxVersion:             VersionTLS13,
			Credential:             &ecdsaP256Certificate,
			SessionTicketsDisabled: true,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte{1, 2, 3, 4},
			},
		},
		resumeSession:           true,
		expectResumeRejected:    true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{

			"-on-retry-expect-early-data-reason", "hello_retry_request",

			"-on-initial-expect-peer-cert-file", rsaCertificate.ChainPath,
			"-on-resume-expect-peer-cert-file", rsaCertificate.ChainPath,
			"-on-retry-expect-peer-cert-file", ecdsaP256Certificate.ChainPath,

			"-on-retry-expect-no-session",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-HRR-RejectTicket-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			MinVersion: VersionTLS13,

			DefaultCurves: []CurveID{},
			Bugs: ProtocolBugs{

				FilterTicket: func(in []byte) ([]byte, error) {
					in[len(in)-1] ^= 1
					return in, nil
				},
			},
		},
		messageCount:            2,
		resumeSession:           true,
		expectResumeRejected:    true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{

			"-on-resume-expect-early-data-reason", "session_not_resumed",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataWithoutResume-Client-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			MaxEarlyDataSize: 16384,
		},
		resumeConfig: &Config{
			MaxVersion:             VersionTLS13,
			SessionTicketsDisabled: true,
			Bugs: ProtocolBugs{
				SendEarlyDataExtension: true,
			},
		},
		resumeSession: true,
		earlyData:     true,
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataVersionDowngrade-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS12,
		},
		resumeSession: true,
		earlyData:     true,
		shouldFail:    true,
		expectedError: ":WRONG_VERSION_ON_EARLY_DATA:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataVersionDowngrade-Client-TLS13-WarningAlert",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendSNIWarningAlert: true,
			},
		},
		resumeSession: true,
		earlyData:     true,
		shouldFail:    true,
		expectedError: ":WRONG_VERSION_ON_EARLY_DATA:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "ServerAcceptsEarlyDataOnHRR-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendHelloRetryRequestCookie: []byte{1, 2, 3, 4},
				SendEarlyDataExtension:      true,
			},
		},
		resumeSession: true,
		earlyData:     true,

		expectEarlyDataRejected: true,
		shouldFail:              true,
		expectedError:           ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "SkipChangeCipherSpec-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SkipChangeCipherSpec: true,
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipChangeCipherSpec-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SkipChangeCipherSpec: true,
			},
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "TooManyChangeCipherSpec-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendExtraChangeCipherSpec: 33,
			},
		},
		shouldFail:    true,
		expectedError: ":TOO_MANY_EMPTY_FRAGMENTS:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TooManyChangeCipherSpec-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendExtraChangeCipherSpec: 33,
			},
		},
		shouldFail:    true,
		expectedError: ":TOO_MANY_EMPTY_FRAGMENTS:",
	})

	testCases = append(testCases, testCase{
		name: "SendPostHandshakeChangeCipherSpec-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendPostHandshakeChangeCipherSpec: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_RECORD:",
		expectedLocalError: "remote error: unexpected message",
	})

	fooString := "foo"
	barString := "bar"

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-ALPNMismatch-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ALPNProtocol: &fooString,
			},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ALPNProtocol: &barString,
			},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-advertise-alpn", "\x03foo\x03bar",

			"-on-retry-expect-early-data-reason", "peer_declined",

			"-on-initial-expect-alpn", "foo",
			"-on-resume-expect-alpn", "foo",
			"-on-retry-expect-alpn", "bar",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-ALPNOmitted1-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"foo"},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-advertise-alpn", "\x03foo\x03bar",

			"-on-retry-expect-early-data-reason", "peer_declined",

			"-on-initial-expect-alpn", "",
			"-on-resume-expect-alpn", "",
			"-on-retry-expect-alpn", "foo",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-ALPNOmitted2-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"foo"},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-advertise-alpn", "\x03foo\x03bar",

			"-on-retry-expect-early-data-reason", "peer_declined",

			"-on-initial-expect-alpn", "foo",
			"-on-resume-expect-alpn", "foo",
			"-on-retry-expect-alpn", "",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-BadALPNMismatch-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ALPNProtocol: &fooString,
			},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysAcceptEarlyData: true,
				ALPNProtocol:          &barString,
			},
		},
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-advertise-alpn", "\x03foo\x03bar",
			"-on-initial-expect-alpn", "foo",
			"-on-resume-expect-alpn", "foo",
			"-on-retry-expect-alpn", "bar",
		},
		shouldFail:         true,
		expectedError:      ":ALPN_MISMATCH_ON_EARLY_DATA:",
		expectedLocalError: "remote error: illegal parameter",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-ALPNPreferenceChanged-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			MaxEarlyDataSize: 16384,
			NextProtos:       []string{"foo", "bar"},
		},
		resumeSession: true,
		flags: []string{
			"-enable-early-data",
			"-expect-ticket-supports-early-data",
			"-expect-no-offer-early-data",

			"-on-initial-advertise-alpn", "\x03foo",
			"-on-initial-expect-alpn", "foo",
			"-on-resume-advertise-alpn", "\x03bar",
			"-on-resume-expect-alpn", "bar",

			"-on-resume-expect-early-data-reason", "alpn_mismatch",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-NonZeroRTTSession-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeSession: true,
		flags: []string{
			"-enable-early-data",
			"-on-resume-expect-no-offer-early-data",

			"-on-resume-expect-early-data-reason", "unsupported_for_session",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-NonZeroRTTSession-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendEarlyData:           [][]byte{{1, 2, 3, 4}},
				ExpectEarlyDataAccepted: false,
			},
		},
		resumeSession: true,

		flags: []string{
			"-on-resume-enable-early-data",
			"-expect-reject-early-data",

			"-on-resume-expect-early-data-reason", "unsupported_for_session",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-ALPNOmitted1-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"foo"},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-initial-select-alpn", "",
			"-on-resume-select-alpn", "foo",
			"-on-resume-expect-early-data-reason", "alpn_mismatch",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-ALPNOmitted2-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"foo"},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-initial-select-alpn", "foo",
			"-on-resume-select-alpn", "",
			"-on-resume-expect-early-data-reason", "alpn_mismatch",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-ALPNMismatch-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"foo"},
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"bar"},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-initial-select-alpn", "foo",
			"-on-resume-select-alpn", "bar",
			"-on-resume-expect-early-data-reason", "alpn_mismatch",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataChannelID-AcceptBoth-Client-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			RequestChannelID: true,
		},
		resumeSession: true,
		earlyData:     true,
		expectations: connectionExpectations{
			channelID: true,
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION_ON_EARLY_DATA:",
		expectedLocalError: "remote error: illegal parameter",
		flags: []string{
			"-send-channel-id", channelIDKeyPath,
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataChannelID-AcceptChannelID-Client-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			RequestChannelID: true,
			Bugs: ProtocolBugs{
				AlwaysRejectEarlyData: true,
			},
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		expectations: connectionExpectations{
			channelID: true,
		},
		flags: []string{
			"-send-channel-id", channelIDKeyPath,

			"-on-retry-expect-early-data-reason", "peer_declined",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataChannelID-AcceptEarlyData-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-send-channel-id", channelIDKeyPath,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyDataChannelID-OfferBoth-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			ChannelID:  &channelIDKey,
		},
		resumeSession:           true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		expectations: connectionExpectations{
			channelID: true,
		},
		flags: []string{
			"-expect-channel-id",
			base64FlagValue(channelIDBytes),
			"-on-resume-expect-early-data-reason", "channel_id",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyDataChannelID-OfferEarlyData-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeSession: true,
		earlyData:     true,
		expectations: connectionExpectations{
			channelID: false,
		},
		flags: []string{
			"-enable-channel-id",
			"-on-resume-expect-early-data-reason", "accept",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-SkipEndOfEarlyData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SkipEndOfEarlyData: true,
			},
		},
		resumeSession:      true,
		earlyData:          true,
		shouldFail:         true,
		expectedLocalError: "remote error: bad record MAC",
		expectedError:      ":BAD_DECRYPT:",
	})

	testCases = append(testCases, testCase{
		protocol: quic,
		testType: serverTest,
		name:     "EarlyData-UnexpectedEndOfEarlyData-QUIC",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendEndOfEarlyDataInQUICAndDTLS: true,
			},
		},
		resumeSession: true,
		earlyData:     true,
		shouldFail:    true,
		expectedError: ":WRONG_ENCRYPTION_LEVEL_RECEIVED:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-UnexpectedHandshake-Server-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendStrayEarlyHandshake: true,
			},
		},
		resumeSession:      true,
		earlyData:          true,
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-Client-VersionAPI-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-expect-version", strconv.Itoa(VersionTLS13),

			"-expect-extended-master-secret",
			"-expect-secure-renegotiation",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-Client-BadFinished-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				BadFinished: true,
			},
		},
		resumeSession:      true,
		earlyData:          true,
		shouldFail:         true,
		expectedError:      ":DIGEST_CHECK_FAILED:",
		expectedLocalError: "remote error: error decrypting message",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyData-Server-BadFinished-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				BadFinished: true,
			},
		},
		resumeSession:      true,
		earlyData:          true,
		shouldFail:         true,
		expectedError:      ":DIGEST_CHECK_FAILED:",
		expectedLocalError: "remote error: error decrypting message",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Server-NonEmptyEndOfEarlyData-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				NonEmptyEndOfEarlyData: true,
			},
		},
		resumeSession: true,
		earlyData:     true,
		shouldFail:    true,
		expectedError: ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ServerSkipCertificateVerify-TLS13",
		config: Config{
			MinVersion: VersionTLS13,
			MaxVersion: VersionTLS13,
			Credential: &rsaChainCertificate,
			Bugs: ProtocolBugs{
				SkipCertificateVerify: true,
			},
		},
		expectations: connectionExpectations{
			peerCertificate: &rsaCertificate,
		},
		shimCertificate: &rsaCertificate,
		flags: []string{
			"-require-any-client-certificate",
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "ClientSkipCertificateVerify-TLS13",
		config: Config{
			MinVersion: VersionTLS13,
			MaxVersion: VersionTLS13,
			Credential: &rsaChainCertificate,
			Bugs: ProtocolBugs{
				SkipCertificateVerify: true,
			},
		},
		expectations: connectionExpectations{
			peerCertificate: &rsaCertificate,
		},
		shimCertificate:    &rsaCertificate,
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "CertificateInResumption-TLS13",
		config: Config{
			MinVersion: VersionTLS13,
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysSendCertificate: true,
			},
		},
		resumeSession:      true,
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "CertificateRequestInResumption-TLS13",
		config: Config{
			MinVersion: VersionTLS13,
			MaxVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
			Bugs: ProtocolBugs{
				AlwaysSendCertificateRequest: true,
			},
		},
		shimCertificate:    &rsaCertificate,
		resumeSession:      true,
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_MESSAGE:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyDataEnabled-Client-MaxTLS12",
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
		flags: []string{
			"-enable-early-data",
			"-max-version", strconv.Itoa(VersionTLS12),
			"-expect-early-data-reason", "protocol_version",
		},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyDataEnabled-Server-MaxTLS12",
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
		flags: []string{
			"-enable-early-data",
			"-max-version", strconv.Itoa(VersionTLS12),
			"-expect-early-data-reason", "protocol_version",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyDataEnabled-Server-NegotiateTLS12",
		config: Config{
			MaxVersion: VersionTLS12,
		},
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
		flags: []string{
			"-enable-early-data",
			"-expect-early-data-reason", "protocol_version",
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-Reject0RTT-DifferentPRF-Client",
		config: Config{
			MaxVersion:   VersionTLS13,
			CipherSuites: []uint16{TLS_AES_128_GCM_SHA256},
		},
		resumeConfig: &Config{
			MaxVersion:   VersionTLS13,
			CipherSuites: []uint16{TLS_AES_256_GCM_SHA384},
		},
		resumeSession:           true,
		expectResumeRejected:    true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-initial-expect-cipher", strconv.Itoa(int(TLS_AES_128_GCM_SHA256)),

			"-on-resume-expect-cipher", strconv.Itoa(int(TLS_AES_128_GCM_SHA256)),
			"-on-retry-expect-cipher", strconv.Itoa(int(TLS_AES_256_GCM_SHA384)),
		},
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-Reject0RTT-DifferentPRF-HRR-Client",
		config: Config{
			MaxVersion:   VersionTLS13,
			CipherSuites: []uint16{TLS_AES_128_GCM_SHA256},
		},
		resumeConfig: &Config{
			MaxVersion:   VersionTLS13,
			CipherSuites: []uint16{TLS_AES_256_GCM_SHA384},

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				ExpectMissingKeyShare: true,
			},
		},
		resumeSession:           true,
		expectResumeRejected:    true,
		earlyData:               true,
		expectEarlyDataRejected: true,
		flags: []string{
			"-on-initial-expect-cipher", strconv.Itoa(int(TLS_AES_128_GCM_SHA256)),

			"-on-resume-expect-cipher", strconv.Itoa(int(TLS_AES_128_GCM_SHA256)),
			"-on-retry-expect-cipher", strconv.Itoa(int(TLS_AES_256_GCM_SHA384)),
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-CipherMismatch-Client-TLS13",
		config: Config{
			MaxVersion:   VersionTLS13,
			CipherSuites: []uint16{TLS_AES_128_GCM_SHA256},
		},
		resumeConfig: &Config{
			MaxVersion:   VersionTLS13,
			CipherSuites: []uint16{TLS_CHACHA20_POLY1305_SHA256},
			Bugs: ProtocolBugs{
				AlwaysAcceptEarlyData: true,
			},
		},
		resumeSession:      true,
		earlyData:          true,
		shouldFail:         true,
		expectedError:      ":CIPHER_MISMATCH_ON_EARLY_DATA:",
		expectedLocalError: "remote error: illegal parameter",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-WriteAfterServerHello",
		config: Config{
			MinVersion: VersionTLS13,
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				ExpectEarlyData:     [][]byte{},
				ExpectLateEarlyData: [][]byte{[]byte(shimInitialWrite)},
			},
		},
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-async",
			"-on-resume-early-write-after-message",
			strconv.Itoa(int(typeServerHello)),
		},
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "EarlyData-WriteAfterEncryptedExtensions",
		config: Config{
			MinVersion: VersionTLS13,
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				ExpectEarlyData:     [][]byte{},
				ExpectLateEarlyData: [][]byte{[]byte(shimInitialWrite)},
			},
		},
		resumeSession: true,
		earlyData:     true,
		flags: []string{
			"-async",
			"-on-resume-early-write-after-message",
			strconv.Itoa(int(typeEncryptedExtensions)),
		},
	})
}

func addTLS13CipherPreferenceTests() {

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TLS13-CipherPreference-Server-ChaCha20-AES",
		config: Config{
			MaxVersion: VersionTLS13,
			CipherSuites: []uint16{
				TLS_CHACHA20_POLY1305_SHA256,
				TLS_AES_128_GCM_SHA256,
			},
			CurvePreferences: []CurveID{CurveX25519},
		},
		flags: []string{
			"-expect-cipher-aes", strconv.Itoa(int(TLS_CHACHA20_POLY1305_SHA256)),
			"-expect-cipher-no-aes", strconv.Itoa(int(TLS_CHACHA20_POLY1305_SHA256)),
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TLS13-CipherPreference-Server-AES-ChaCha20",
		config: Config{
			MaxVersion: VersionTLS13,
			CipherSuites: []uint16{
				TLS_AES_128_GCM_SHA256,
				TLS_CHACHA20_POLY1305_SHA256,
			},
			CurvePreferences: []CurveID{CurveX25519},
		},
		flags: []string{
			"-expect-cipher-aes", strconv.Itoa(int(TLS_AES_128_GCM_SHA256)),
			"-expect-cipher-no-aes", strconv.Itoa(int(TLS_CHACHA20_POLY1305_SHA256)),
		},
	})

	testCases = append(testCases, testCase{
		name: "TLS13-CipherPreference-Client",
		config: Config{
			MaxVersion: VersionTLS13,

			PreferServerCipherSuites: false,
		},
		flags: []string{
			"-expect-cipher-aes", strconv.Itoa(int(TLS_AES_128_GCM_SHA256)),
			"-expect-cipher-no-aes", strconv.Itoa(int(TLS_CHACHA20_POLY1305_SHA256)),
		},
	})
}
