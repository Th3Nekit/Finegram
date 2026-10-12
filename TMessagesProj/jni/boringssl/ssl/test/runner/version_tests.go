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

func addVersionNegotiationTests() {
	for _, protocol := range []protocol{tls, dtls, quic} {
		for _, shimVers := range allVersions(protocol) {

			var flags []string
			for _, vers := range allVersions(protocol) {
				if vers.version > shimVers.version {
					flags = append(flags, vers.excludeFlag)
				}
			}

			flags2 := []string{"-max-version", shimVers.shimFlag(protocol)}

			for _, runnerVers := range allVersions(protocol) {
				expectedVersion := shimVers.version
				if runnerVers.version < shimVers.version {
					expectedVersion = runnerVers.version
				}

				suffix := shimVers.name + "-" + runnerVers.name
				suffix += "-" + protocol.String()

				clientVers := shimVers.version
				if clientVers > VersionTLS10 {
					clientVers = VersionTLS10
				}
				clientVers = recordVersionToWire(clientVers, protocol)
				serverVers := expectedVersion
				if expectedVersion >= VersionTLS13 {
					serverVers = VersionTLS12
				}
				serverVers = recordVersionToWire(serverVers, protocol)

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: clientTest,
					name:     "VersionNegotiation-Client-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
						Bugs: ProtocolBugs{
							ExpectInitialRecordVersion: clientVers,
						},
					},
					flags: flags,
					expectations: connectionExpectations{
						version: expectedVersion,
					},

					skipVersionNameCheck: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: clientTest,
					name:     "VersionNegotiation-Client2-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
						Bugs: ProtocolBugs{
							ExpectInitialRecordVersion: clientVers,
						},
					},
					flags: flags2,
					expectations: connectionExpectations{
						version: expectedVersion,
					},
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "VersionNegotiation-Server-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
						Bugs: ProtocolBugs{
							ExpectInitialRecordVersion: serverVers,
						},
					},
					flags: flags,
					expectations: connectionExpectations{
						version: expectedVersion,
					},

					skipVersionNameCheck: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "VersionNegotiation-Server2-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
						Bugs: ProtocolBugs{
							ExpectInitialRecordVersion: serverVers,
						},
					},
					flags: flags2,
					expectations: connectionExpectations{
						version: expectedVersion,
					},
				})
			}
		}
	}

	for _, protocol := range []protocol{tls, dtls, quic} {
		for _, vers := range allVersions(protocol) {
			suffix := vers.name + "-" + protocol.String()

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     "VersionNegotiationExtension-" + suffix,
				config: Config{
					Bugs: ProtocolBugs{
						SendSupportedVersions:      []uint16{0x1111, vers.wire(protocol), 0x2222},
						IgnoreTLS13DowngradeRandom: true,
					},
				},
				expectations: connectionExpectations{
					version: vers.version,
				},
			})
		}
	}

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoSupportedVersions",
		config: Config{
			Bugs: ProtocolBugs{
				SendSupportedVersions: []uint16{0x1111},
			},
		},
		shouldFail:    true,
		expectedError: ":UNSUPPORTED_PROTOCOL:",
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "NoSupportedVersions-DTLS",
		config: Config{
			Bugs: ProtocolBugs{
				SendSupportedVersions: []uint16{0x1111},
			},
		},
		shouldFail:    true,
		expectedError: ":UNSUPPORTED_PROTOCOL:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ClientHelloVersionTooHigh",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendClientVersion:          0x0304,
				OmitSupportedVersions:      true,
				IgnoreTLS13DowngradeRandom: true,
			},
		},
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ConflictingVersionNegotiation",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:          VersionTLS12,
				SendSupportedVersions:      []uint16{VersionTLS11},
				IgnoreTLS13DowngradeRandom: true,
			},
		},

		expectations: connectionExpectations{
			version: VersionTLS11,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ConflictingVersionNegotiation-2",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:          VersionTLS11,
				SendSupportedVersions:      []uint16{VersionTLS12},
				IgnoreTLS13DowngradeRandom: true,
			},
		},

		expectations: connectionExpectations{
			version: VersionTLS12,
		},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "SupportedVersionSelection-TLS12",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendServerSupportedVersionExtension: VersionTLS12,
			},
		},
		shouldFail:    true,
		expectedError: ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "IgnoreClientVersionOrder",
		config: Config{
			Bugs: ProtocolBugs{
				SendSupportedVersions: []uint16{VersionTLS12, VersionTLS13},
			},
		},
		expectations: connectionExpectations{
			version: VersionTLS13,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "MinorVersionTolerance",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:          0x03ff,
				OmitSupportedVersions:      true,
				IgnoreTLS13DowngradeRandom: true,
			},
		},
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "MajorVersionTolerance",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:          0x0400,
				OmitSupportedVersions:      true,
				IgnoreTLS13DowngradeRandom: true,
			},
		},

		expectations: connectionExpectations{
			version: VersionTLS12,
		},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "VersionTolerance-TLS13",
		config: Config{
			Bugs: ProtocolBugs{

				SendClientVersion: 0x0400,
			},
		},
		expectations: connectionExpectations{
			version: VersionTLS13,
		},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "MinorVersionTolerance-DTLS",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:          0xfe00,
				OmitSupportedVersions:      true,
				IgnoreTLS13DowngradeRandom: true,
			},
		},
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "MajorVersionTolerance-DTLS",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:          0xfdff,
				OmitSupportedVersions:      true,
				IgnoreTLS13DowngradeRandom: true,
			},
		},
		expectations: connectionExpectations{
			version: VersionTLS12,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "VersionTooLow",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:     0x0200,
				OmitSupportedVersions: true,
			},
		},
		shouldFail:    true,
		expectedError: ":UNSUPPORTED_PROTOCOL:",
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "VersionTooLow-DTLS",
		config: Config{
			Bugs: ProtocolBugs{
				SendClientVersion:     0xffff,
				OmitSupportedVersions: true,
			},
		},
		shouldFail:    true,
		expectedError: ":UNSUPPORTED_PROTOCOL:",
	})

	testCases = append(testCases, testCase{
		name: "ServerBogusVersion",
		config: Config{
			Bugs: ProtocolBugs{
				SendServerHelloVersion: 0x1234,
			},
		},
		shouldFail:    true,
		expectedError: ":UNSUPPORTED_PROTOCOL:",
	})

	for _, protocol := range []protocol{tls, dtls} {
		for _, vers := range allVersions(protocol) {
			if vers.version >= VersionTLS13 {
				continue
			}
			clientShimError := "tls: downgrade from TLS 1.3 detected"
			if vers.version < VersionTLS12 {
				clientShimError = "tls: downgrade from TLS 1.2 detected"
			}

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     "Downgrade-" + vers.name + "-Client-" + protocol.String(),
				config: Config{
					Bugs: ProtocolBugs{
						NegotiateVersion: vers.wire(protocol),
					},
				},
				expectations: connectionExpectations{
					version: vers.version,
				},
				shouldFail:         true,
				expectedError:      ":TLS13_DOWNGRADE:",
				expectedLocalError: "remote error: illegal parameter",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				testType: serverTest,
				name:     "Downgrade-" + vers.name + "-Server-" + protocol.String(),
				config: Config{
					Bugs: ProtocolBugs{
						SendSupportedVersions: []uint16{vers.wire(protocol)},
					},
				},
				expectations: connectionExpectations{
					version: vers.version,
				},
				shouldFail:         true,
				expectedLocalError: clientShimError,
			})
		}
	}

	testCases = append(testCases, testCase{
		name: "NoSSL3-Client",
		config: Config{
			MinVersion: VersionSSL30,
			MaxVersion: VersionSSL30,
		},
		shouldFail:         true,
		expectedLocalError: "tls: client did not offer any supported protocol versions",
	})
	testCases = append(testCases, testCase{
		name: "NoSSL3-Client-Unsolicited",
		config: Config{
			MinVersion: VersionSSL30,
			MaxVersion: VersionSSL30,
			Bugs: ProtocolBugs{

				NegotiateVersion: VersionSSL30,
			},
		},
		shouldFail:         true,
		expectedError:      ":UNSUPPORTED_PROTOCOL:",
		expectedLocalError: "remote error: protocol version not supported",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoSSL3-Server",
		config: Config{
			MinVersion: VersionSSL30,
			MaxVersion: VersionSSL30,
		},
		shouldFail:         true,
		expectedError:      ":UNSUPPORTED_PROTOCOL:",
		expectedLocalError: "remote error: protocol version not supported",
	})
}

func addMinimumVersionTests() {
	for _, protocol := range []protocol{tls, dtls, quic} {
		for _, shimVers := range allVersions(protocol) {

			var flags []string
			for _, vers := range allVersions(protocol) {
				if vers.version < shimVers.version {
					flags = append(flags, vers.excludeFlag)
				}
			}

			flags2 := []string{"-min-version", shimVers.shimFlag(protocol)}

			for _, runnerVers := range allVersions(protocol) {
				suffix := shimVers.name + "-" + runnerVers.name
				suffix += "-" + protocol.String()

				var expectedVersion uint16
				var shouldFail bool
				var expectedError, expectedLocalError string
				if runnerVers.version >= shimVers.version {
					expectedVersion = runnerVers.version
				} else {
					shouldFail = true
					expectedError = ":UNSUPPORTED_PROTOCOL:"
					expectedLocalError = "remote error: protocol version not supported"
				}

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: clientTest,
					name:     "MinimumVersion-Client-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
						Bugs: ProtocolBugs{

							NegotiateVersion:            runnerVers.wire(protocol),
							IgnorePeerCipherPreferences: shouldFail,
						},
					},
					flags: flags,
					expectations: connectionExpectations{
						version: expectedVersion,
					},
					shouldFail:         shouldFail,
					expectedError:      expectedError,
					expectedLocalError: expectedLocalError,

					skipVersionNameCheck: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: clientTest,
					name:     "MinimumVersion-Client2-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
						Bugs: ProtocolBugs{

							NegotiateVersion:            runnerVers.wire(protocol),
							IgnorePeerCipherPreferences: shouldFail,
						},
					},
					flags: flags2,
					expectations: connectionExpectations{
						version: expectedVersion,
					},
					shouldFail:         shouldFail,
					expectedError:      expectedError,
					expectedLocalError: expectedLocalError,
				})

				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "MinimumVersion-Server-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
					},
					flags: flags,
					expectations: connectionExpectations{
						version: expectedVersion,
					},
					shouldFail:         shouldFail,
					expectedError:      expectedError,
					expectedLocalError: expectedLocalError,

					skipVersionNameCheck: true,
				})
				testCases = append(testCases, testCase{
					protocol: protocol,
					testType: serverTest,
					name:     "MinimumVersion-Server2-" + suffix,
					config: Config{
						MaxVersion: runnerVers.version,
					},
					flags: flags2,
					expectations: connectionExpectations{
						version: expectedVersion,
					},
					shouldFail:         shouldFail,
					expectedError:      expectedError,
					expectedLocalError: expectedLocalError,
				})
			}
		}
	}
}

func addRecordVersionTests() {
	for _, ver := range tlsVersions {

		testCases = append(testCases, testCase{
			name: "CheckRecordVersion-" + ver.name,
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Bugs: ProtocolBugs{
					SendRecordVersion: 0x03ff,
				},
			},
			shouldFail:    true,
			expectedError: ":WRONG_VERSION_NUMBER:",
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "LooseInitialRecordVersion-" + ver.name,
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Bugs: ProtocolBugs{
					SendInitialRecordVersion: 0x03ff,
				},
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "GarbageInitialRecordVersion-" + ver.name,
			config: Config{
				MinVersion: ver.version,
				MaxVersion: ver.version,
				Bugs: ProtocolBugs{
					SendInitialRecordVersion: 0xffff,
				},
			},
			shouldFail:    true,
			expectedError: ":WRONG_VERSION_NUMBER:",
		})
	}
}
