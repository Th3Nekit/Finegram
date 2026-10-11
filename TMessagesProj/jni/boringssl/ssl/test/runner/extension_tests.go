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

import (
	"crypto/x509"
	"crypto/x509/pkix"
	"fmt"
	"math/big"
	"time"
)

func addExtensionTests() {
	exampleCertificate := generateSingleCertChain(&x509.Certificate{
		SerialNumber: big.NewInt(57005),
		Subject: pkix.Name{
			CommonName: "test cert",
		},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(time.Hour),
		DNSNames:              []string{"example.com"},
		IsCA:                  true,
		BasicConstraintsValid: true,
	}, &ecdsaP256Key)

	for _, protocol := range []protocol{tls, dtls, quic} {
		for _, ver := range allVersions(protocol) {
			suffix := fmt.Sprintf("%s-%s", protocol.String(), ver.name)

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: clientTest,
				name:     "DuplicateExtensionClient-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						DuplicateExtension: true,
					},
				},
				shouldFail:         true,
				expectedLocalError: "remote error: error decoding message",
			})
			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     "DuplicateExtensionServer-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						DuplicateExtension: true,
					},
				},
				shouldFail:         true,
				expectedLocalError: "remote error: error decoding message",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: clientTest,
				name:     "ServerNameExtensionClient-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						ExpectServerName: "example.com",
					},
					Credential: &exampleCertificate,
				},
				flags: []string{"-host-name", "example.com"},
			})
			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: clientTest,
				name:     "ServerNameExtensionClientMismatch-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						ExpectServerName: "mismatch.com",
					},
				},
				flags:              []string{"-host-name", "example.com"},
				shouldFail:         true,
				expectedLocalError: "tls: unexpected server name",
			})
			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: clientTest,
				name:     "ServerNameExtensionClientMissing-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						ExpectServerName: "missing.com",
					},
				},
				shouldFail:         true,
				expectedLocalError: "tls: unexpected server name",
			})
			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: clientTest,
				name:     "TolerateServerNameAck-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						SendServerNameAck: true,
					},
					Credential: &exampleCertificate,
				},
				flags:         []string{"-host-name", "example.com"},
				resumeSession: true,
			})
			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: clientTest,
				name:     "UnsolicitedServerNameAck-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						SendServerNameAck: true,
					},
				},
				shouldFail:         true,
				expectedError:      ":UNEXPECTED_EXTENSION:",
				expectedLocalError: "remote error: unsupported extension",
			})
			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     "ServerNameExtensionServer-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					ServerName: "example.com",
				},
				flags:         []string{"-expect-server-name", "example.com"},
				resumeSession: true,
			})

			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           clientTest,
				skipQUICALPNConfig: true,
				name:               "ALPNClient-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{"foo"},
				},
				flags: []string{
					"-advertise-alpn", "\x03foo\x03bar\x03baz",
					"-expect-alpn", "foo",
				},
				expectations: connectionExpectations{
					nextProto:     "foo",
					nextProtoType: alpn,
				},
				resumeSession: true,
			})
			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           clientTest,
				skipQUICALPNConfig: true,
				name:               "ALPNClient-RejectUnknown-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						SendALPN: "baz",
					},
				},
				flags: []string{
					"-advertise-alpn", "\x03foo\x03bar",
				},
				shouldFail:         true,
				expectedError:      ":INVALID_ALPN_PROTOCOL:",
				expectedLocalError: "remote error: illegal parameter",
			})
			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           clientTest,
				skipQUICALPNConfig: true,
				name:               "ALPNClient-AllowUnknown-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						SendALPN: "baz",
					},
				},
				flags: []string{
					"-advertise-alpn", "\x03foo\x03bar",
					"-allow-unknown-alpn-protos",
					"-expect-alpn", "baz",
				},
			})
			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           serverTest,
				skipQUICALPNConfig: true,
				name:               "ALPNServer-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{"foo", "bar", "baz"},
				},
				flags: []string{
					"-expect-advertised-alpn", "\x03foo\x03bar\x03baz",
					"-select-alpn", "foo",
				},
				expectations: connectionExpectations{
					nextProto:     "foo",
					nextProtoType: alpn,
				},
				resumeSession: true,
			})

			var shouldDeclineALPNFail bool
			var declineALPNError, declineALPNLocalError string
			if protocol == quic {

				shouldDeclineALPNFail = true
				declineALPNError = ":NO_APPLICATION_PROTOCOL:"
				declineALPNLocalError = "remote error: no application protocol"
			}
			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           serverTest,
				skipQUICALPNConfig: true,
				name:               "ALPNServer-Decline-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{"foo", "bar", "baz"},
				},
				flags: []string{"-decline-alpn"},
				expectations: connectionExpectations{
					noNextProto: true,
				},
				resumeSession:      true,
				shouldFail:         shouldDeclineALPNFail,
				expectedError:      declineALPNError,
				expectedLocalError: declineALPNLocalError,
			})

			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           serverTest,
				skipQUICALPNConfig: true,
				name:               "ALPNServer-Reject-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{"foo", "bar", "baz"},
				},
				flags:              []string{"-reject-alpn"},
				shouldFail:         true,
				expectedError:      ":NO_APPLICATION_PROTOCOL:",
				expectedLocalError: "remote error: no application protocol",
			})

			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           serverTest,
				skipQUICALPNConfig: true,
				name:               "ALPNServer-SelectEmpty-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{"foo", "bar", "baz"},
				},
				flags: []string{
					"-expect-advertised-alpn", "\x03foo\x03bar\x03baz",
					"-select-empty-alpn",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: internal error",
				expectedError:      ":INVALID_ALPN_PROTOCOL:",
			})

			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           serverTest,
				skipQUICALPNConfig: true,
				name:               "ALPNServer-Async-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{"foo", "bar", "baz"},

					SessionTicketsDisabled: ver.version < VersionTLS13,
				},
				flags: []string{
					"-expect-advertised-alpn", "\x03foo\x03bar\x03baz",
					"-select-alpn", "foo",
					"-async",
				},
				expectations: connectionExpectations{
					nextProto:     "foo",
					nextProtoType: alpn,
				},
				resumeSession: true,
			})

			var emptyString string
			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           clientTest,
				skipQUICALPNConfig: true,
				name:               "ALPNClient-EmptyProtocolName-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					NextProtos: []string{""},
					Bugs: ProtocolBugs{

						ALPNProtocol: &emptyString,
					},
				},
				flags: []string{
					"-advertise-alpn", "\x03foo",
				},
				shouldFail:    true,
				expectedError: ":PARSE_TLSEXT:",
			})
			testCases = append(testCases, testCase{
				protocol:           protocol,
				testType:           serverTest,
				skipQUICALPNConfig: true,
				name:               "ALPNServer-EmptyProtocolName-" + suffix,
				config: Config{
					MaxVersion: ver.version,

					NextProtos: []string{"foo", "", "baz"},
				},
				flags: []string{
					"-select-alpn", "foo",
				},
				shouldFail:    true,
				expectedError: ":PARSE_TLSEXT:",
			})

			if ver.version < VersionTLS13 && protocol == tls {

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "ALPNServer-Preferred-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						NextProtos: []string{"foo", "bar", "baz"},
					},
					flags: []string{
						"-expect-advertised-alpn", "\x03foo\x03bar\x03baz",
						"-select-alpn", "foo",
						"-advertise-npn", "\x03foo\x03bar\x03baz",
					},
					expectations: connectionExpectations{
						nextProto:     "foo",
						nextProtoType: alpn,
					},
					resumeSession: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "ALPNServer-Preferred-Swapped-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						NextProtos: []string{"foo", "bar", "baz"},
						Bugs: ProtocolBugs{
							SwapNPNAndALPN: true,
						},
					},
					flags: []string{
						"-expect-advertised-alpn", "\x03foo\x03bar\x03baz",
						"-select-alpn", "foo",
						"-advertise-npn", "\x03foo\x03bar\x03baz",
					},
					expectations: connectionExpectations{
						nextProto:     "foo",
						nextProtoType: alpn,
					},
					resumeSession: true,
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     "NegotiateALPNAndNPN-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						NextProtos: []string{"foo", "bar", "baz"},
						Bugs: ProtocolBugs{
							NegotiateALPNAndNPN: true,
						},
					},
					flags: []string{
						"-advertise-alpn", "\x03foo",
						"-select-next-proto", "foo",
					},
					shouldFail:    true,
					expectedError: ":NEGOTIATED_BOTH_NPN_AND_ALPN:",
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     "NegotiateALPNAndNPN-Swapped-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						NextProtos: []string{"foo", "bar", "baz"},
						Bugs: ProtocolBugs{
							NegotiateALPNAndNPN: true,
							SwapNPNAndALPN:      true,
						},
					},
					flags: []string{
						"-advertise-alpn", "\x03foo",
						"-select-next-proto", "foo",
					},
					shouldFail:    true,
					expectedError: ":NEGOTIATED_BOTH_NPN_AND_ALPN:",
				})
			}

			if protocol == quic {
				testCases = append(testCases, testCase{
					testType: clientTest,
					protocol: protocol,
					name:     "Client-ALPNMissingFromConfig-" + suffix,
					config: Config{
						MinVersion: ver.version,
						MaxVersion: ver.version,
					},
					skipQUICALPNConfig: true,
					shouldFail:         true,
					expectedError:      ":NO_APPLICATION_PROTOCOL:",
				})
				testCases = append(testCases, testCase{
					testType: clientTest,
					protocol: protocol,
					name:     "Client-ALPNMissing-" + suffix,
					config: Config{
						MinVersion: ver.version,
						MaxVersion: ver.version,
					},
					flags: []string{
						"-advertise-alpn", "\x03foo",
					},
					skipQUICALPNConfig: true,
					shouldFail:         true,
					expectedError:      ":NO_APPLICATION_PROTOCOL:",
					expectedLocalError: "remote error: no application protocol",
				})
				testCases = append(testCases, testCase{
					testType: serverTest,
					protocol: protocol,
					name:     "Server-ALPNMissing-" + suffix,
					config: Config{
						MinVersion: ver.version,
						MaxVersion: ver.version,
					},
					skipQUICALPNConfig: true,
					shouldFail:         true,
					expectedError:      ":NO_APPLICATION_PROTOCOL:",
					expectedLocalError: "remote error: no application protocol",
				})
				testCases = append(testCases, testCase{
					testType: serverTest,
					protocol: protocol,
					name:     "Server-ALPNMismatch-" + suffix,
					config: Config{
						MinVersion: ver.version,
						MaxVersion: ver.version,
						NextProtos: []string{"foo"},
					},
					flags: []string{
						"-decline-alpn",
					},
					skipQUICALPNConfig: true,
					shouldFail:         true,
					expectedError:      ":NO_APPLICATION_PROTOCOL:",
					expectedLocalError: "remote error: no application protocol",
				})
			}

			if ver.version >= VersionTLS13 {

				for _, alpsCodePoint := range []ALPSUseCodepoint{ALPSUseCodepointNew, ALPSUseCodepointOld} {
					flags := []string{}
					expectations := connectionExpectations{
						peerApplicationSettingsOld: []byte("shim1"),
					}
					resumeExpectations := &connectionExpectations{
						peerApplicationSettingsOld: []byte("shim2"),
					}

					if alpsCodePoint == ALPSUseCodepointNew {
						flags = append(flags, "-alps-use-new-codepoint")
						expectations = connectionExpectations{
							peerApplicationSettings: []byte("shim1"),
						}
						resumeExpectations = &connectionExpectations{
							peerApplicationSettings: []byte("shim2"),
						}
					}

					flags = append(flags,
						"-advertise-alpn", "\x05proto",
						"-expect-alpn", "proto",
						"-on-initial-application-settings", "proto,shim1",
						"-on-initial-expect-peer-application-settings", "runner1",
						"-on-resume-application-settings", "proto,shim2",
						"-on-resume-expect-peer-application-settings", "runner2")

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           clientTest,
						name:               fmt.Sprintf("ALPS-Basic-Client-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner1")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeConfig: &Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner2")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeSession:      true,
						expectations:       expectations,
						resumeExpectations: resumeExpectations,
						flags:              flags,
					})

					flags = []string{}
					expectations = connectionExpectations{
						peerApplicationSettingsOld: []byte("shim1"),
					}
					resumeExpectations = &connectionExpectations{
						peerApplicationSettingsOld: []byte("shim2"),
					}

					if alpsCodePoint == ALPSUseCodepointNew {
						flags = append(flags, "-alps-use-new-codepoint")
						expectations = connectionExpectations{
							peerApplicationSettings: []byte("shim1"),
						}
						resumeExpectations = &connectionExpectations{
							peerApplicationSettings: []byte("shim2"),
						}
					}

					flags = append(flags,
						"-select-alpn", "proto",
						"-on-initial-application-settings", "proto,shim1",
						"-on-initial-expect-peer-application-settings", "runner1",
						"-on-resume-application-settings", "proto,shim2",
						"-on-resume-expect-peer-application-settings", "runner2")

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-Basic-Server-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner1")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeConfig: &Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner2")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeSession:      true,
						expectations:       expectations,
						resumeExpectations: resumeExpectations,
						flags:              flags,
					})

					alpsFlags := []string{}
					expectations = connectionExpectations{
						peerApplicationSettingsOld: []byte("shim1"),
					}
					resumeExpectations = &connectionExpectations{
						peerApplicationSettingsOld: []byte("shim2"),
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						alpsFlags = append(alpsFlags, "-alps-use-new-codepoint")
						expectations = connectionExpectations{
							peerApplicationSettings: []byte("shim1"),
						}
						resumeExpectations = &connectionExpectations{
							peerApplicationSettings: []byte("shim2"),
						}
					}

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-Basic-Server-Defer-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner1")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeConfig: &Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner2")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeSession:      true,
						expectations:       expectations,
						resumeExpectations: resumeExpectations,
						flags: append([]string{
							"-select-alpn", "proto",
							"-defer-alps",
							"-on-initial-application-settings", "proto,shim1",
							"-on-initial-expect-peer-application-settings", "runner1",
							"-on-resume-application-settings", "proto,shim2",
							"-on-resume-expect-peer-application-settings", "runner2",
						}, alpsFlags...),
					})

					expectations = connectionExpectations{
						peerApplicationSettingsOld: []byte{},
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						expectations = connectionExpectations{
							peerApplicationSettings: []byte{},
						}
					}

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           clientTest,
						name:               fmt.Sprintf("ALPS-Empty-Client-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": {}},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeSession: true,
						expectations:  expectations,
						flags: append([]string{
							"-advertise-alpn", "\x05proto",
							"-expect-alpn", "proto",
							"-application-settings", "proto,",
							"-expect-peer-application-settings", "",
						}, alpsFlags...),
					})
					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-Empty-Server-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": {}},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeSession: true,
						expectations:  expectations,
						flags: append([]string{
							"-select-alpn", "proto",
							"-application-settings", "proto,",
							"-expect-peer-application-settings", "",
						}, alpsFlags...),
					})

					bugs := ProtocolBugs{
						AlwaysNegotiateApplicationSettingsOld: true,
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						bugs = ProtocolBugs{
							AlwaysNegotiateApplicationSettingsNew: true,
						}
					}

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           clientTest,
						name:               fmt.Sprintf("ALPS-UnsupportedProtocol-Client-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto1"},
							ApplicationSettings: map[string][]byte{"proto1": []byte("runner")},
							Bugs:                bugs,
							ALPSUseNewCodepoint: alpsCodePoint,
						},

						flags: append([]string{
							"-advertise-alpn", "\x06proto1\x06proto2",
							"-application-settings", "proto2,shim",
							"-expect-alpn", "proto1",
						}, alpsFlags...),

						shouldFail:         true,
						expectedError:      ":INVALID_ALPN_PROTOCOL:",
						expectedLocalError: "remote error: illegal parameter",
					})

					bugs = ProtocolBugs{
						AlwaysNegotiateApplicationSettingsOld: true,
					}
					if alpsCodePoint == ALPSUseCodepointOld {
						bugs = ProtocolBugs{
							AlwaysNegotiateApplicationSettingsNew: true,
						}
					}

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           clientTest,
						name:               fmt.Sprintf("ALPS-WrongServerCodepoint-Client-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": {}},
							Bugs:                bugs,
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						flags: append([]string{
							"-advertise-alpn", "\x05proto",
							"-expect-alpn", "proto",
							"-application-settings", "proto,",
							"-expect-peer-application-settings", "",
						}, alpsFlags...),
						shouldFail:         true,
						expectedError:      ":UNEXPECTED_EXTENSION:",
						expectedLocalError: "remote error: unsupported extension",
					})

					clientSends := ALPSUseCodepointNew
					if alpsCodePoint == ALPSUseCodepointNew {
						clientSends = ALPSUseCodepointOld
					}

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-IgnoreClientWrongCodepoint-Server-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner1")},
							ALPSUseNewCodepoint: clientSends,
						},
						resumeConfig: &Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner2")},
							ALPSUseNewCodepoint: clientSends,
						},
						resumeSession: true,
						flags: append([]string{
							"-select-alpn", "proto",
							"-on-initial-application-settings", "proto,shim1",
							"-on-resume-application-settings", "proto,shim2",
						}, alpsFlags...),
					})

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-UnsupportedProtocol-Server-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto1"},
							ApplicationSettings: map[string][]byte{"proto1": []byte("runner")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},

						flags: append([]string{
							"-select-alpn", "proto1",
							"-application-settings", "proto2,shim",
						}, alpsFlags...),
					})

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           clientTest,
						name:               fmt.Sprintf("ALPS-UnsupportedProtocol-Client-ServerBoth-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto1"},
							ApplicationSettings: map[string][]byte{"proto1": []byte("runner")},
							Bugs: ProtocolBugs{
								AlwaysNegotiateApplicationSettingsBoth: true,
							},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						flags: append([]string{
							"-advertise-alpn", "\x06proto1\x06proto2",
							"-application-settings", "proto1,shim",
							"-expect-alpn", "proto1",
						}, alpsFlags...),

						shouldFail:         true,
						expectedError:      ":UNEXPECTED_EXTENSION:",
						expectedLocalError: "remote error: unsupported extension",
					})

					expectations = connectionExpectations{
						peerApplicationSettingsOld: []byte("shim"),
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						expectations = connectionExpectations{
							peerApplicationSettings: []byte("shim"),
						}
					}

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-OmitClientApplicationSettings-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner")},
							Bugs: ProtocolBugs{
								OmitClientApplicationSettings: true,
							},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						flags: append([]string{
							"-select-alpn", "proto",
							"-application-settings", "proto,shim",
						}, alpsFlags...),

						expectations:       expectations,
						shouldFail:         true,
						expectedError:      ":MISSING_EXTENSION:",
						expectedLocalError: "remote error: missing extension",
					})

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ALPS-OmitClientEncryptedExtensions-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner")},
							Bugs: ProtocolBugs{
								OmitClientEncryptedExtensions: true,
							},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						flags: append([]string{
							"-select-alpn", "proto",
							"-application-settings", "proto,shim",
						}, alpsFlags...),

						expectations:       expectations,
						shouldFail:         true,
						expectedError:      ":UNEXPECTED_MESSAGE:",
						expectedLocalError: "remote error: unexpected message",
					})

					testCases = append(testCases, testCase{
						protocol: protocol,
						testType: serverTest,
						name:     fmt.Sprintf("UnexpectedClientEncryptedExtensions-%s-%s", alpsCodePoint, suffix),
						config: Config{
							MaxVersion: ver.version,
							Bugs: ProtocolBugs{
								AlwaysSendClientEncryptedExtensions: true,
							},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						shouldFail:         true,
						expectedError:      ":UNEXPECTED_MESSAGE:",
						expectedLocalError: "remote error: unexpected message",
					})

					testCases = append(testCases, testCase{
						protocol:           protocol,
						testType:           serverTest,
						name:               fmt.Sprintf("ExtraClientEncryptedExtension-%s-%s", alpsCodePoint, suffix),
						skipQUICALPNConfig: true,
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"proto"},
							ApplicationSettings: map[string][]byte{"proto": []byte("runner")},
							Bugs: ProtocolBugs{
								SendExtraClientEncryptedExtension: true,
							},
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						flags: append([]string{
							"-select-alpn", "proto",
							"-application-settings", "proto,shim",
						}, alpsFlags...),

						expectations:       expectations,
						shouldFail:         true,
						expectedError:      ":UNEXPECTED_EXTENSION:",
						expectedLocalError: "remote error: unsupported extension",
					})

					if protocol != dtls {
						for _, empty := range []bool{false, true} {
							maybeEmpty := ""
							runnerSettings := "runner"
							shimSettings := "shim"
							if empty {
								maybeEmpty = "Empty-"
								runnerSettings = ""
								shimSettings = ""
							}

							expectations = connectionExpectations{
								peerApplicationSettingsOld: []byte(shimSettings),
							}
							if alpsCodePoint == ALPSUseCodepointNew {
								expectations = connectionExpectations{
									peerApplicationSettings: []byte(shimSettings),
								}
							}
							testCases = append(testCases, testCase{
								protocol:           protocol,
								testType:           clientTest,
								name:               fmt.Sprintf("ALPS-EarlyData-Client-%s-%s-%s", alpsCodePoint, maybeEmpty, suffix),
								skipQUICALPNConfig: true,
								config: Config{
									MaxVersion:          ver.version,
									NextProtos:          []string{"proto"},
									ApplicationSettings: map[string][]byte{"proto": []byte(runnerSettings)},
									ALPSUseNewCodepoint: alpsCodePoint,
								},
								resumeSession: true,
								earlyData:     true,
								flags: append([]string{
									"-advertise-alpn", "\x05proto",
									"-expect-alpn", "proto",
									"-application-settings", "proto," + shimSettings,
									"-expect-peer-application-settings", runnerSettings,
								}, alpsFlags...),
								expectations: expectations,
							})
							testCases = append(testCases, testCase{
								protocol:           protocol,
								testType:           serverTest,
								name:               fmt.Sprintf("ALPS-EarlyData-Server-%s-%s-%s", alpsCodePoint, maybeEmpty, suffix),
								skipQUICALPNConfig: true,
								config: Config{
									MaxVersion:          ver.version,
									NextProtos:          []string{"proto"},
									ApplicationSettings: map[string][]byte{"proto": []byte(runnerSettings)},
									ALPSUseNewCodepoint: alpsCodePoint,
								},
								resumeSession: true,
								earlyData:     true,
								flags: append([]string{
									"-select-alpn", "proto",
									"-application-settings", "proto," + shimSettings,
									"-expect-peer-application-settings", runnerSettings,
								}, alpsFlags...),
								expectations: expectations,
							})

							testCases = append(testCases, testCase{
								protocol:           protocol,
								testType:           clientTest,
								name:               fmt.Sprintf("ALPS-EarlyData-SendApplicationSettingsWithEarlyData-Client-%s-%s-%s", alpsCodePoint, maybeEmpty, suffix),
								skipQUICALPNConfig: true,
								config: Config{
									MaxVersion:          ver.version,
									NextProtos:          []string{"proto"},
									ApplicationSettings: map[string][]byte{"proto": []byte(runnerSettings)},
									Bugs: ProtocolBugs{
										SendApplicationSettingsWithEarlyData: true,
									},
									ALPSUseNewCodepoint: alpsCodePoint,
								},
								resumeSession: true,
								earlyData:     true,
								flags: append([]string{
									"-advertise-alpn", "\x05proto",
									"-expect-alpn", "proto",
									"-application-settings", "proto," + shimSettings,
									"-expect-peer-application-settings", runnerSettings,
								}, alpsFlags...),
								expectations:       expectations,
								shouldFail:         true,
								expectedError:      ":UNEXPECTED_EXTENSION_ON_EARLY_DATA:",
								expectedLocalError: "remote error: illegal parameter",
							})
							testCases = append(testCases, testCase{
								protocol:           protocol,
								testType:           serverTest,
								name:               fmt.Sprintf("ALPS-EarlyData-SendApplicationSettingsWithEarlyData-Server-%s-%s-%s", alpsCodePoint, maybeEmpty, suffix),
								skipQUICALPNConfig: true,
								config: Config{
									MaxVersion:          ver.version,
									NextProtos:          []string{"proto"},
									ApplicationSettings: map[string][]byte{"proto": []byte(runnerSettings)},
									Bugs: ProtocolBugs{
										SendApplicationSettingsWithEarlyData: true,
									},
									ALPSUseNewCodepoint: alpsCodePoint,
								},
								resumeSession: true,
								earlyData:     true,
								flags: append([]string{
									"-select-alpn", "proto",
									"-application-settings", "proto," + shimSettings,
									"-expect-peer-application-settings", runnerSettings,
								}, alpsFlags...),
								expectations:       expectations,
								shouldFail:         true,
								expectedError:      ":UNEXPECTED_MESSAGE:",
								expectedLocalError: "remote error: unexpected message",
							})
						}

						alpsMismatchTests := []struct {
							name                            string
							initialSettings, resumeSettings []byte
						}{
							{"DifferentValues", []byte("settings1"), []byte("settings2")},
							{"OnOff", []byte("settings"), nil},
							{"OffOn", nil, []byte("settings")},

							{"OnEmpty", []byte("settings"), []byte{}},
							{"EmptyOn", []byte{}, []byte("settings")},
							{"EmptyOff", []byte{}, nil},
							{"OffEmpty", nil, []byte{}},
						}
						for _, test := range alpsMismatchTests {
							flags := []string{"-on-resume-expect-early-data-reason", "alps_mismatch"}
							flags = append(flags, alpsFlags...)
							if test.initialSettings != nil {
								flags = append(flags, "-on-initial-application-settings", "proto,"+string(test.initialSettings))
								flags = append(flags, "-on-initial-expect-peer-application-settings", "runner")
							}
							if test.resumeSettings != nil {
								flags = append(flags, "-on-resume-application-settings", "proto,"+string(test.resumeSettings))
								flags = append(flags, "-on-resume-expect-peer-application-settings", "runner")
							}

							expectations = connectionExpectations{
								peerApplicationSettingsOld: test.initialSettings,
							}
							resumeExpectations = &connectionExpectations{
								peerApplicationSettingsOld: test.resumeSettings,
							}
							if alpsCodePoint == ALPSUseCodepointNew {
								expectations = connectionExpectations{
									peerApplicationSettings: test.initialSettings,
								}
								resumeExpectations = &connectionExpectations{
									peerApplicationSettings: test.resumeSettings,
								}
							}

							if test.initialSettings != nil {
								testCases = append(testCases, testCase{
									protocol:           protocol,
									testType:           clientTest,
									name:               fmt.Sprintf("ALPS-EarlyData-Mismatch-%s-Client-%s-%s", test.name, alpsCodePoint, suffix),
									skipQUICALPNConfig: true,
									config: Config{
										MaxVersion:          ver.version,
										MaxEarlyDataSize:    16384,
										NextProtos:          []string{"proto"},
										ApplicationSettings: map[string][]byte{"proto": []byte("runner")},
										ALPSUseNewCodepoint: alpsCodePoint,
									},
									resumeSession: true,
									flags: append([]string{
										"-enable-early-data",
										"-expect-ticket-supports-early-data",
										"-expect-no-offer-early-data",
										"-advertise-alpn", "\x05proto",
										"-expect-alpn", "proto",
									}, flags...),
									expectations:       expectations,
									resumeExpectations: resumeExpectations,
								})
							}

							testCases = append(testCases, testCase{
								protocol:           protocol,
								testType:           serverTest,
								name:               fmt.Sprintf("ALPS-EarlyData-Mismatch-%s-Server-%s-%s", test.name, alpsCodePoint, suffix),
								skipQUICALPNConfig: true,
								config: Config{
									MaxVersion:          ver.version,
									NextProtos:          []string{"proto"},
									ApplicationSettings: map[string][]byte{"proto": []byte("runner")},
									ALPSUseNewCodepoint: alpsCodePoint,
								},
								resumeSession:           true,
								earlyData:               true,
								expectEarlyDataRejected: true,
								flags: append([]string{
									"-select-alpn", "proto",
								}, flags...),
								expectations:       expectations,
								resumeExpectations: resumeExpectations,
							})
						}

						testCases = append(testCases, testCase{
							protocol:           protocol,
							testType:           clientTest,
							name:               fmt.Sprintf("ALPS-EarlyData-Client-ServerDecline-%s-%s", alpsCodePoint, suffix),
							skipQUICALPNConfig: true,
							config: Config{
								MaxVersion:          ver.version,
								NextProtos:          []string{"proto"},
								ALPSUseNewCodepoint: alpsCodePoint,
							},
							resumeSession: true,
							earlyData:     true,
							flags: append([]string{
								"-advertise-alpn", "\x05proto",
								"-expect-alpn", "proto",
								"-application-settings", "proto,shim",
							}, alpsFlags...),
						})
						testCases = append(testCases, testCase{
							protocol:           protocol,
							testType:           serverTest,
							name:               fmt.Sprintf("ALPS-EarlyData-Server-ClientNoOffe-%s-%s", alpsCodePoint, suffix),
							skipQUICALPNConfig: true,
							config: Config{
								MaxVersion:          ver.version,
								NextProtos:          []string{"proto"},
								ALPSUseNewCodepoint: alpsCodePoint,
							},
							resumeSession: true,
							earlyData:     true,
							flags: append([]string{
								"-select-alpn", "proto",
								"-application-settings", "proto,shim",
							}, alpsFlags...),
						})
					}
				}
			} else {

				for _, alpsCodePoint := range []ALPSUseCodepoint{ALPSUseCodepointNew, ALPSUseCodepointOld} {
					flags := []string{
						"-advertise-alpn", "\x03foo",
						"-expect-alpn", "foo",
						"-application-settings", "foo,shim",
					}
					bugs := ProtocolBugs{
						AlwaysNegotiateApplicationSettingsOld: true,
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						flags = append(flags, "-alps-use-new-codepoint")
						bugs = ProtocolBugs{
							AlwaysNegotiateApplicationSettingsNew: true,
						}
					}
					testCases = append(testCases, testCase{
						protocol: protocol,
						testType: clientTest,
						name:     fmt.Sprintf("ALPS-Reject-Client-%s-%s", alpsCodePoint, suffix),
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"foo"},
							ApplicationSettings: map[string][]byte{"foo": []byte("runner")},
							Bugs:                bugs,
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						flags:              flags,
						shouldFail:         true,
						expectedError:      ":UNEXPECTED_EXTENSION:",
						expectedLocalError: "remote error: unsupported extension",
					})

					flags = []string{
						"-on-resume-advertise-alpn", "\x03foo",
						"-on-resume-expect-alpn", "foo",
						"-on-resume-application-settings", "foo,shim",
					}
					bugs = ProtocolBugs{
						AlwaysNegotiateApplicationSettingsOld: true,
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						flags = append(flags, "-alps-use-new-codepoint")
						bugs = ProtocolBugs{
							AlwaysNegotiateApplicationSettingsNew: true,
						}
					}
					testCases = append(testCases, testCase{
						protocol: protocol,
						testType: clientTest,
						name:     fmt.Sprintf("ALPS-Reject-Client-Resume-%s-%s", alpsCodePoint, suffix),
						config: Config{
							MaxVersion: ver.version,
						},
						resumeConfig: &Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"foo"},
							ApplicationSettings: map[string][]byte{"foo": []byte("runner")},
							Bugs:                bugs,
							ALPSUseNewCodepoint: alpsCodePoint,
						},
						resumeSession:      true,
						flags:              flags,
						shouldFail:         true,
						expectedError:      ":UNEXPECTED_EXTENSION:",
						expectedLocalError: "remote error: unsupported extension",
					})

					flags = []string{
						"-select-alpn", "foo",
						"-application-settings", "foo,shim",
					}
					if alpsCodePoint == ALPSUseCodepointNew {
						flags = append(flags, "-alps-use-new-codepoint")
					}
					testCases = append(testCases, testCase{
						protocol: protocol,
						testType: serverTest,
						name:     fmt.Sprintf("ALPS-Decline-Server-%s-%s", alpsCodePoint, suffix),
						config: Config{
							MaxVersion:          ver.version,
							NextProtos:          []string{"foo"},
							ApplicationSettings: map[string][]byte{"foo": []byte("runner")},
							ALPSUseNewCodepoint: alpsCodePoint,
						},

						resumeSession: true,
						flags:         flags,

					})
				}
			}

			if protocol == quic {

				for _, clientConfig := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy} {
					for _, serverSends := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy, QUICUseCodepointBoth, QUICUseCodepointNeither} {
						useCodepointFlag := "0"
						if clientConfig == QUICUseCodepointLegacy {
							useCodepointFlag = "1"
						}
						flags := []string{
							"-quic-transport-params",
							base64FlagValue([]byte{1, 2}),
							"-quic-use-legacy-codepoint", useCodepointFlag,
						}
						expectations := connectionExpectations{
							quicTransportParams: []byte{1, 2},
						}
						shouldFail := false
						expectedError := ""
						expectedLocalError := ""
						if clientConfig == QUICUseCodepointLegacy {
							expectations = connectionExpectations{
								quicTransportParamsLegacy: []byte{1, 2},
							}
						}
						if serverSends != clientConfig {
							expectations = connectionExpectations{}
							shouldFail = true
							if serverSends == QUICUseCodepointNeither {
								expectedError = ":MISSING_EXTENSION:"
							} else {
								expectedLocalError = "remote error: unsupported extension"
							}
						} else {
							flags = append(flags,
								"-expect-quic-transport-params",
								base64FlagValue([]byte{3, 4}))
						}
						testCases = append(testCases, testCase{
							testType: clientTest,
							protocol: protocol,
							name:     fmt.Sprintf("QUICTransportParams-Client-Client%s-Server%s-%s", clientConfig, serverSends, suffix),
							config: Config{
								MinVersion:                            ver.version,
								MaxVersion:                            ver.version,
								QUICTransportParams:                   []byte{3, 4},
								QUICTransportParamsUseLegacyCodepoint: serverSends,
							},
							flags:                     flags,
							expectations:              expectations,
							shouldFail:                shouldFail,
							expectedError:             expectedError,
							expectedLocalError:        expectedLocalError,
							skipTransportParamsConfig: true,
						})
					}
				}

				for _, clientSends := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy, QUICUseCodepointBoth, QUICUseCodepointNeither} {
					for _, serverConfig := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy} {
						expectations := connectionExpectations{
							quicTransportParams: []byte{3, 4},
						}
						shouldFail := false
						expectedError := ""
						useCodepointFlag := "0"
						if serverConfig == QUICUseCodepointLegacy {
							useCodepointFlag = "1"
							expectations = connectionExpectations{
								quicTransportParamsLegacy: []byte{3, 4},
							}
						}
						flags := []string{
							"-quic-transport-params",
							base64FlagValue([]byte{3, 4}),
							"-quic-use-legacy-codepoint", useCodepointFlag,
						}
						if clientSends != QUICUseCodepointBoth && clientSends != serverConfig {
							expectations = connectionExpectations{}
							shouldFail = true
							expectedError = ":MISSING_EXTENSION:"
						} else {
							flags = append(flags,
								"-expect-quic-transport-params",
								base64FlagValue([]byte{1, 2}),
							)
						}
						testCases = append(testCases, testCase{
							testType: serverTest,
							protocol: protocol,
							name:     fmt.Sprintf("QUICTransportParams-Server-Client%s-Server%s-%s", clientSends, serverConfig, suffix),
							config: Config{
								MinVersion:                            ver.version,
								MaxVersion:                            ver.version,
								QUICTransportParams:                   []byte{1, 2},
								QUICTransportParamsUseLegacyCodepoint: clientSends,
							},
							flags:                     flags,
							expectations:              expectations,
							shouldFail:                shouldFail,
							expectedError:             expectedError,
							skipTransportParamsConfig: true,
						})
					}
				}
			} else {

				for _, clientConfig := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy} {
					useCodepointFlag := "0"
					if clientConfig == QUICUseCodepointLegacy {
						useCodepointFlag = "1"
					}
					testCases = append(testCases, testCase{
						protocol: protocol,
						testType: clientTest,
						name:     fmt.Sprintf("QUICTransportParams-Client-NotSentInNonQUIC-%s-%s", clientConfig, suffix),
						config: Config{
							MinVersion:                            ver.version,
							MaxVersion:                            ver.version,
							QUICTransportParamsUseLegacyCodepoint: clientConfig,
						},
						flags: []string{
							"-max-version",
							ver.shimFlag(protocol),
							"-quic-transport-params",
							base64FlagValue([]byte{3, 4}),
							"-quic-use-legacy-codepoint", useCodepointFlag,
						},
						shouldFail:                true,
						expectedError:             ":QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED:",
						skipTransportParamsConfig: true,
					})
				}

				for _, clientSends := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy, QUICUseCodepointBoth, QUICUseCodepointNeither} {
					for _, serverConfig := range []QUICUseCodepoint{QUICUseCodepointStandard, QUICUseCodepointLegacy} {
						shouldFail := false
						expectedLocalError := ""
						useCodepointFlag := "0"
						if serverConfig == QUICUseCodepointLegacy {
							useCodepointFlag = "1"
						}
						if clientSends == QUICUseCodepointStandard || clientSends == QUICUseCodepointBoth {
							shouldFail = true
							expectedLocalError = "remote error: unsupported extension"
						}
						testCases = append(testCases, testCase{
							protocol: protocol,
							testType: serverTest,
							name:     fmt.Sprintf("QUICTransportParams-NonQUICServer-Client%s-Server%s-%s", clientSends, serverConfig, suffix),
							config: Config{
								MinVersion:                            ver.version,
								MaxVersion:                            ver.version,
								QUICTransportParams:                   []byte{1, 2},
								QUICTransportParamsUseLegacyCodepoint: clientSends,
							},
							flags: []string{
								"-quic-use-legacy-codepoint", useCodepointFlag,
							},
							shouldFail:                shouldFail,
							expectedLocalError:        expectedLocalError,
							skipTransportParamsConfig: true,
						})
					}
				}

			}

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     "CorruptTicket-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						FilterTicket: func(in []byte) ([]byte, error) {
							in[len(in)-1] ^= 1
							return in, nil
						},
					},
				},
				resumeSession:        true,
				expectResumeRejected: true,
			})

			for _, aeadCallback := range []bool{false, true} {
				flag := "-use-ticket-callback"
				callbackSuffix := suffix
				if aeadCallback {
					flag = "-use-ticket-aead-callback"
					callbackSuffix += "-AEAD"
				}
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketCallback-" + callbackSuffix,
					config: Config{
						MaxVersion: ver.version,
					},
					resumeSession: true,
					flags:         []string{flag},
				})

				if !aeadCallback {
					testCases = append(testCases, testCase{
						protocol: protocol,
						testType: serverTest,
						name:     "TicketCallback-Renew-" + callbackSuffix,
						config: Config{
							MaxVersion: ver.version,
							Bugs: ProtocolBugs{
								ExpectNewTicket: true,
							},
						},
						flags:         []string{flag, "-renew-ticket"},
						resumeSession: true,
					})
				}
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketCallback-Skip-" + callbackSuffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							ExpectNoNonEmptyNewSessionTicket: true,
						},
					},
					flags: []string{flag, "-skip-ticket"},
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketCallback-SingleCall-" + callbackSuffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							FilterTicket: func(in []byte) ([]byte, error) {
								in[len(in)-1] ^= 1
								return in, nil
							},
						},
					},
					resumeSession:        true,
					expectResumeRejected: true,
					flags: []string{
						flag,
						"-async",
					},
				})
			}

			if ver.version < VersionTLS13 {
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketSessionIDLength-0-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							EmptyTicketSessionID: true,
						},
					},
					resumeSession: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketSessionIDLength-16-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							TicketSessionIDLength: 16,
						},
					},
					resumeSession: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketSessionIDLength-32-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							TicketSessionIDLength: 32,
						},
					},
					resumeSession: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "TicketSessionIDLength-33-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							TicketSessionIDLength: 33,
						},
					},
					resumeSession: true,
					shouldFail:    true,

					expectedError: ":CLIENTHELLO_PARSE_FAILED:",
				})
			}

			if protocol == dtls {
				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     "SRTP-Client-" + suffix,
					config: Config{
						MaxVersion:             ver.version,
						SRTPProtectionProfiles: []uint16{40, SRTP_AES128_CM_HMAC_SHA1_80, 42},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32",
					},
					expectations: connectionExpectations{
						srtpProtectionProfile: SRTP_AES128_CM_HMAC_SHA1_80,
					},
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "SRTP-Server-" + suffix,
					config: Config{
						MaxVersion:             ver.version,
						SRTPProtectionProfiles: []uint16{40, SRTP_AES128_CM_HMAC_SHA1_80, 42},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32",
					},
					expectations: connectionExpectations{
						srtpProtectionProfile: SRTP_AES128_CM_HMAC_SHA1_80,
					},
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "SRTP-Server-IgnoreMKI-" + suffix,
					config: Config{
						MaxVersion:             ver.version,
						SRTPProtectionProfiles: []uint16{SRTP_AES128_CM_HMAC_SHA1_80},
						Bugs: ProtocolBugs{
							SRTPMasterKeyIdentifier: "bogus",
						},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32",
					},
					expectations: connectionExpectations{
						srtpProtectionProfile: SRTP_AES128_CM_HMAC_SHA1_80,
					},
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "SRTP-Server-NoMatch-" + suffix,
					config: Config{
						MaxVersion:             ver.version,
						SRTPProtectionProfiles: []uint16{100, 101, 102},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32",
					},
					expectations: connectionExpectations{
						srtpProtectionProfile: 0,
					},
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     "SRTP-Client-NoMatch-" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Bugs: ProtocolBugs{
							SendSRTPProtectionProfile: SRTP_AES128_CM_HMAC_SHA1_32,
						},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80",
					},
					shouldFail:    true,
					expectedError: ":BAD_SRTP_PROTECTION_PROFILE_LIST:",
				})
			} else {

				testCases = append(testCases, testCase{
					protocol: protocol,
					name:     "SRTP-Client-Ignore-" + suffix,
					config: Config{
						MaxVersion:             ver.version,
						SRTPProtectionProfiles: []uint16{40, SRTP_AES128_CM_HMAC_SHA1_80, 42},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32",
					},
					expectations: connectionExpectations{
						srtpProtectionProfile: 0,
					},
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "SRTP-Server-Ignore-" + suffix,
					config: Config{
						MaxVersion:             ver.version,
						SRTPProtectionProfiles: []uint16{40, SRTP_AES128_CM_HMAC_SHA1_80, 42},
					},
					flags: []string{
						"-srtp-profiles",
						"SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32",
					},
					expectations: connectionExpectations{
						srtpProtectionProfile: 0,
					},
				})
			}

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "SignedCertificateTimestampList-Client-" + suffix,
				testType: clientTest,
				config: Config{
					MaxVersion: ver.version,
					Credential: rsaCertificate.WithSCTList(testSCTList),
				},
				flags: []string{
					"-enable-signed-cert-timestamps",
					"-expect-signed-cert-timestamps",
					base64FlagValue(testSCTList),
				},
				resumeSession: true,
			})

			var differentSCTList []byte
			differentSCTList = append(differentSCTList, testSCTList...)
			differentSCTList[len(differentSCTList)-1] ^= 1

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "SendSCTListOnResume-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Credential: rsaCertificate.WithSCTList(testSCTList),
					Bugs: ProtocolBugs{
						SendSCTListOnResume: differentSCTList,
					},
				},
				flags: []string{
					"-enable-signed-cert-timestamps",
					"-expect-signed-cert-timestamps",
					base64FlagValue(testSCTList),
				},
				resumeSession: true,
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "SignedCertificateTimestampList-Server-" + suffix,
				testType: serverTest,
				config: Config{
					MaxVersion: ver.version,
				},
				shimCertificate: rsaCertificate.WithSCTList(testSCTList),
				expectations: connectionExpectations{
					peerCertificate: rsaCertificate.WithSCTList(testSCTList),
				},
				resumeSession: true,
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "SignedCertificateTimestampListEmpty-Client-" + suffix,
				testType: clientTest,
				config: Config{
					MaxVersion: ver.version,
					Credential: rsaCertificate.WithSCTList([]byte{0, 0}),
				},
				flags: []string{
					"-enable-signed-cert-timestamps",
				},
				shouldFail:    true,
				expectedError: ":ERROR_PARSING_EXTENSION:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "SignedCertificateTimestampListEmptySCT-Client-" + suffix,
				testType: clientTest,
				config: Config{
					MaxVersion: ver.version,
					Credential: rsaCertificate.WithSCTList([]byte{0, 6, 0, 2, 1, 2, 0, 0}),
				},
				flags: []string{
					"-enable-signed-cert-timestamps",
				},
				shouldFail:    true,
				expectedError: ":ERROR_PARSING_EXTENSION:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     "UnsolicitedCertificateExtensions-" + suffix,
				config: Config{
					MaxVersion: ver.version,
					Bugs: ProtocolBugs{
						NoOCSPStapling:                true,
						NoSignedCertificateTimestamps: true,
					},
				},
				shimCertificate: rsaCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
			})

			for _, ech := range []bool{false, true} {
				if ech && ver.version < VersionTLS13 {
					continue
				}

				test := testCase{
					protocol:           protocol,
					name:               "AllExtensions-Client-Permute",
					skipQUICALPNConfig: true,
					config: Config{
						MinVersion:          ver.version,
						MaxVersion:          ver.version,
						Credential:          rsaCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
						NextProtos:          []string{"proto"},
						ApplicationSettings: map[string][]byte{"proto": []byte("runner1")},
						Bugs: ProtocolBugs{
							SendServerNameAck: true,
							ExpectServerName:  "example.com",
							ExpectGREASE:      true,
						},
					},
					resumeSession: true,
					flags: []string{
						"-permute-extensions",
						"-enable-grease",
						"-enable-ocsp-stapling",
						"-enable-signed-cert-timestamps",
						"-advertise-alpn", "\x05proto",
						"-expect-alpn", "proto",
						"-host-name", "example.com",
					},
				}

				if ech {
					test.name += "-ECH"
					echConfig := generateServerECHConfig(&ECHConfig{ConfigID: 42})
					test.config.ServerECHConfigs = []ServerECHConfig{echConfig}
					test.flags = append(test.flags,
						"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
						"-expect-ech-accept",
					)
					test.expectations.echAccepted = true
				}

				if ver.version >= VersionTLS13 {

					test.name += "-HelloRetryRequest"

					test.config.ApplicationSettings = map[string][]byte{"proto": []byte("runner")}
					test.flags = append(test.flags,
						"-application-settings", "proto,shim",
						"-alps-use-new-codepoint",
						"-expect-peer-application-settings", "runner")
					test.expectations.peerApplicationSettings = []byte("shim")
				}

				if protocol == dtls {
					test.config.SRTPProtectionProfiles = []uint16{SRTP_AES128_CM_HMAC_SHA1_80}
					test.flags = append(test.flags, "-srtp-profiles", "SRTP_AES128_CM_SHA1_80")
					test.expectations.srtpProtectionProfile = SRTP_AES128_CM_HMAC_SHA1_80
				}

				test.name += "-" + suffix
				testCases = append(testCases, test)
			}
		}
	}

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "ClientHelloPadding",
		config: Config{
			Bugs: ProtocolBugs{
				RequireClientHelloSize: 512,
			},
		},

		flags: []string{"-host-name", "01234567890123456789012345678901234567890123456789012345678901234567890123456789.com"},
	})

	testCases = append(testCases, testCase{
		name: "NPN-Forbidden-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"foo"},
			Bugs: ProtocolBugs{
				NegotiateNPNAtAllVersions: true,
			},
		},
		flags:         []string{"-select-next-proto", "foo"},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "EMS-Forbidden-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				NegotiateEMSAtAllVersions: true,
			},
		},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "RenegotiationInfo-Forbidden-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				NegotiateRenegotiationInfoAtAllVersions: true,
			},
		},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "Ticket-Forbidden-TLS13",
		config: Config{
			MaxVersion: VersionTLS12,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AdvertiseTicketExtension: true,
			},
		},
		resumeSession: true,
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NPN-Declined-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			NextProtos: []string{"bar"},
		},
		flags: []string{"-advertise-npn", "\x03foo\x03bar\x03baz"},
	})

	testCases = append(testCases, testCase{
		name: "SendOCSPResponseOnResume-TLS12",
		config: Config{
			MaxVersion: VersionTLS12,
			Credential: rsaCertificate.WithOCSP(testOCSPResponse),
			Bugs: ProtocolBugs{
				SendOCSPResponseOnResume: []byte("bogus"),
			},
		},
		flags: []string{
			"-enable-ocsp-stapling",
			"-expect-ocsp-response",
			base64FlagValue(testOCSPResponse),
		},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		name: "SendUnsolicitedOCSPOnCertificate-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendExtensionOnCertificate: testOCSPExtension,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "SendUnsolicitedSCTOnCertificate-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendExtensionOnCertificate: testSCTExtension,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name:     "SendExtensionOnClientCertificate-TLS13",
		testType: serverTest,
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: &rsaCertificate,
			Bugs: ProtocolBugs{
				SendExtensionOnCertificate: testOCSPExtension,
			},
		},
		flags: []string{
			"-enable-ocsp-stapling",
			"-require-any-client-certificate",
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "SendUnknownExtensionOnCertificate-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendExtensionOnCertificate: []byte{0x00, 0x7f, 0, 0},
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "IgnoreExtensionsOnIntermediates-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: rsaChainCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
			Bugs: ProtocolBugs{

				SendOCSPOnIntermediates: testOCSPResponse2,
				SendSCTOnIntermediates:  testSCTList2,
			},
		},
		flags: []string{
			"-enable-ocsp-stapling",
			"-expect-ocsp-response",
			base64FlagValue(testOCSPResponse),
			"-enable-signed-cert-timestamps",
			"-expect-signed-cert-timestamps",
			base64FlagValue(testSCTList),
		},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "SendNoExtensionsOnIntermediate-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectNoExtensionsOnIntermediate: true,
			},
		},
		shimCertificate: rsaChainCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
	})

	testCases = append(testCases, testCase{
		name: "SendNoClientCertificateExtensions-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
		},
		shimCertificate: rsaChainCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
	})

	testCases = append(testCases, testCase{
		name: "SendDuplicateExtensionsOnCerts-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: rsaCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
			Bugs: ProtocolBugs{
				SendDuplicateCertExtensions: true,
			},
		},
		flags: []string{
			"-enable-ocsp-stapling",
			"-enable-signed-cert-timestamps",
		},
		resumeSession: true,
		shouldFail:    true,
		expectedError: ":DUPLICATE_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name:            "SignedCertificateTimestampListInvalid-Server",
		testType:        serverTest,
		shimCertificate: rsaCertificate.WithSCTList([]byte{0, 0}),
		shouldFail:      true,
		expectedError:   ":INVALID_SCT_LIST:",
	})
}

func addUnknownExtensionTests() {

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "UnknownExtension-Client",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				CustomExtension: "custom extension",
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "UnknownExtension-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				CustomExtension: "custom extension",
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "UnknownUnencryptedExtension-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				CustomUnencryptedExtension: "custom extension",
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",

		expectedLocalError: "local error: bad record MAC",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "UnexpectedUnencryptedExtension-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendUnencryptedALPN: "foo",
			},
		},
		flags: []string{
			"-advertise-alpn", "\x03foo\x03bar",
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",

		expectedLocalError: "local error: bad record MAC",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "UnofferedExtension-Client",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendALPN: "alpn",
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "UnofferedExtension-Client-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendALPN: "alpn",
			},
		},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})
}

func addOmitExtensionsTests() {

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ExpectOmitExtensions",
		config: Config{
			MinVersion: VersionTLS12,
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				ExpectOmitExtensions: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "tls: ServerHello did not omit extensions",
	})

	for _, ver := range tlsVersions {
		if ver.version > VersionTLS12 {
			continue
		}

		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "OmitExtensions-ClientHello-" + ver.name,
			config: Config{
				MinVersion:             ver.version,
				MaxVersion:             ver.version,
				SessionTicketsDisabled: true,
				Bugs: ProtocolBugs{
					OmitExtensions: true,

					ExpectOmitExtensions: true,
				},
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "EmptyExtensions-ClientHello-" + ver.name,
			config: Config{
				MinVersion:             ver.version,
				MaxVersion:             ver.version,
				SessionTicketsDisabled: true,
				Bugs: ProtocolBugs{
					EmptyExtensions: true,

					ExpectOmitExtensions: true,
				},
			},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			name:     "OmitExtensions-ServerHello-" + ver.name,
			config: Config{
				MinVersion:             ver.version,
				MaxVersion:             ver.version,
				SessionTicketsDisabled: true,
				Bugs: ProtocolBugs{
					OmitExtensions: true,

					NoExtendedMasterSecret:        true,
					NoRenegotiationInfo:           true,
					NoOCSPStapling:                true,
					NoSignedCertificateTimestamps: true,
				},
			},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			name:     "EmptyExtensions-ServerHello-" + ver.name,
			config: Config{
				MinVersion:             ver.version,
				MaxVersion:             ver.version,
				SessionTicketsDisabled: true,
				Bugs: ProtocolBugs{
					EmptyExtensions: true,

					NoExtendedMasterSecret:        true,
					NoRenegotiationInfo:           true,
					NoOCSPStapling:                true,
					NoSignedCertificateTimestamps: true,
				},
			},
		})
	}
}
