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

import "slices"

func addChangeCipherSpecTests() {

	testCases = append(testCases, testCase{
		name: "SkipChangeCipherSpec-Client",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SkipChangeCipherSpec: true,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipChangeCipherSpec-Server",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SkipChangeCipherSpec: true,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SkipChangeCipherSpec-Server-NPN",
		config: Config{
			MaxVersion: VersionTLS12,
			NextProtos: []string{"bar"},
			Bugs: ProtocolBugs{
				SkipChangeCipherSpec: true,
			},
		},
		flags: []string{
			"-advertise-npn", "\x03foo\x03bar\x03baz",
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
	})

	for _, packed := range []bool{false, true} {
		var suffix string
		if packed {
			suffix = "-Packed"
		}

		testCases = append(testCases, testCase{
			name: "FragmentAcrossChangeCipherSpec-Client" + suffix,
			config: Config{
				MaxVersion: VersionTLS12,
				Bugs: ProtocolBugs{
					FragmentAcrossChangeCipherSpec: true,
					PackHandshakeFlight:            packed,
				},
			},
			shouldFail:    true,
			expectedError: ":UNEXPECTED_RECORD:",
		})
		testCases = append(testCases, testCase{
			name: "FragmentAcrossChangeCipherSpec-Client-Resume" + suffix,
			config: Config{
				MaxVersion: VersionTLS12,
			},
			resumeSession: true,
			resumeConfig: &Config{
				MaxVersion: VersionTLS12,
				Bugs: ProtocolBugs{
					FragmentAcrossChangeCipherSpec: true,
					PackHandshakeFlight:            packed,
				},
			},
			shouldFail:    true,
			expectedError: ":UNEXPECTED_RECORD:",
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "FragmentAcrossChangeCipherSpec-Server" + suffix,
			config: Config{
				MaxVersion: VersionTLS12,
				Bugs: ProtocolBugs{
					FragmentAcrossChangeCipherSpec: true,
					PackHandshakeFlight:            packed,
				},
			},
			shouldFail:    true,
			expectedError: ":UNEXPECTED_RECORD:",
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "FragmentAcrossChangeCipherSpec-Server-Resume" + suffix,
			config: Config{
				MaxVersion: VersionTLS12,
			},
			resumeSession: true,
			resumeConfig: &Config{
				MaxVersion: VersionTLS12,
				Bugs: ProtocolBugs{
					FragmentAcrossChangeCipherSpec: true,
					PackHandshakeFlight:            packed,
				},
			},
			shouldFail:    true,
			expectedError: ":UNEXPECTED_RECORD:",
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "FragmentAcrossChangeCipherSpec-Server-NPN" + suffix,
			config: Config{
				MaxVersion: VersionTLS12,
				NextProtos: []string{"bar"},
				Bugs: ProtocolBugs{
					FragmentAcrossChangeCipherSpec: true,
					PackHandshakeFlight:            packed,
				},
			},
			flags: []string{
				"-advertise-npn", "\x03foo\x03bar\x03baz",
			},
			shouldFail:    true,
			expectedError: ":UNEXPECTED_RECORD:",
		})
	}

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PartialClientFinishedWithClientHello-TLS12-Resume",
		config: Config{
			MaxVersion: VersionTLS12,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				PartialClientFinishedWithClientHello: true,
			},
		},
		resumeSession:      true,
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "PartialFinishedWithServerHelloDone",
		config: Config{
			MaxVersion:             VersionTLS12,
			SessionTicketsDisabled: true,
			Bugs: ProtocolBugs{
				PartialFinishedWithServerHelloDone: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "KeyChangeWithBufferedMessages-DTLS",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					next = slices.Clone(next)
					slices.Reverse(next)
					for i := range next {
						next[i].Epoch = 0
					}
					c.WriteFlight(next)
				},
			},
		},
		shouldFail:    true,
		expectedError: ":EXCESS_HANDSHAKE_DATA:",
	})

	testCases = append(testCases, testCase{
		name: "PartialEncryptedExtensionsWithServerHello",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				PartialEncryptedExtensionsWithServerHello: true,
			},
		},
		shouldFail:    true,
		expectedError: ":EXCESS_HANDSHAKE_DATA:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PartialClientFinishedWithClientHello",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				PartialClientFinishedWithClientHello: true,
			},
		},
		shouldFail:    true,
		expectedError: ":EXCESS_HANDSHAKE_DATA:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PartialClientFinishedWithSecondClientHello",
		config: Config{
			MaxVersion: VersionTLS13,

			DefaultCurves: []CurveID{},
			Bugs: ProtocolBugs{
				PartialClientFinishedWithSecondClientHello: true,
			},
		},
		shouldFail:    true,
		expectedError: ":EXCESS_HANDSHAKE_DATA:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PartialEndOfEarlyDataWithClientHello",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				PartialEndOfEarlyDataWithClientHello: true,
			},
		},
		resumeSession: true,
		earlyData:     true,
		shouldFail:    true,
		expectedError: ":EXCESS_HANDSHAKE_DATA:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyChangeCipherSpec-server-1",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				EarlyChangeCipherSpec: 1,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "EarlyChangeCipherSpec-server-2",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				EarlyChangeCipherSpec: 2,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_RECORD:",
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "StrayChangeCipherSpec",
		config: Config{

			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					c.WriteFragments([]DTLSFragment{{IsChangeCipherSpec: true, Data: []byte{1}}})
					c.WriteFlight(next)
				},
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "BadChangeCipherSpec-1",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				BadChangeCipherSpec: []byte{2},
			},
		},
		shouldFail:    true,
		expectedError: ":BAD_CHANGE_CIPHER_SPEC:",
	})
	testCases = append(testCases, testCase{
		name: "BadChangeCipherSpec-2",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				BadChangeCipherSpec: []byte{1, 1},
			},
		},
		shouldFail:    true,
		expectedError: ":BAD_CHANGE_CIPHER_SPEC:",
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "BadChangeCipherSpec-DTLS-1",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				BadChangeCipherSpec: []byte{2},
			},
		},
		shouldFail:    true,
		expectedError: ":BAD_CHANGE_CIPHER_SPEC:",
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "BadChangeCipherSpec-DTLS-2",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				BadChangeCipherSpec: []byte{1, 1},
			},
		},
		shouldFail:    true,
		expectedError: ":BAD_CHANGE_CIPHER_SPEC:",
	})
}
