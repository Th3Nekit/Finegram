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
	"fmt"
	"strconv"
)

var testCurves = []struct {
	name string
	id   CurveID
}{
	{"P-224", CurveP224},
	{"P-256", CurveP256},
	{"P-384", CurveP384},
	{"P-521", CurveP521},
	{"X25519", CurveX25519},
	{"Kyber", CurveX25519Kyber768},
	{"MLKEM", CurveX25519MLKEM768},
}

const bogusCurve = 0x1234

func isPqGroup(r CurveID) bool {
	return r == CurveX25519Kyber768 || r == CurveX25519MLKEM768
}

func isECDHGroup(r CurveID) bool {
	return r == CurveP224 || r == CurveP256 || r == CurveP384 || r == CurveP521
}

func isX25519Group(r CurveID) bool {
	return r == CurveX25519 || r == CurveX25519Kyber768 || r == CurveX25519MLKEM768
}

func addCurveTests() {

	ecdheCiphers := []uint16{
		TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
		TLS_AES_256_GCM_SHA384,
	}

	for _, curve := range testCurves {
		for _, ver := range tlsVersions {
			if isPqGroup(curve.id) && ver.version < VersionTLS13 {
				continue
			}
			for _, testType := range []testType{clientTest, serverTest} {
				suffix := fmt.Sprintf("%s-%s-%s", testType, curve.name, ver.name)

				testCases = append(testCases, testCase{
					testType: testType,
					name:     "CurveTest-" + suffix,
					config: Config{
						MaxVersion:       ver.version,
						CipherSuites:     ecdheCiphers,
						CurvePreferences: []CurveID{curve.id},
					},
					flags: append(
						[]string{"-expect-curve-id", strconv.Itoa(int(curve.id))},
						flagInts("-curves", shimConfig.AllCurves)...,
					),
					expectations: connectionExpectations{
						curveID: curve.id,
					},
				})

				badKeyShareLocalError := "remote error: illegal parameter"
				if testType == clientTest && ver.version >= VersionTLS13 {

					badKeyShareLocalError = "local error: bad record MAC"
				}

				testCases = append(testCases, testCase{
					testType: testType,
					name:     "CurveTest-Invalid-TruncateKeyShare-" + suffix,
					config: Config{
						MaxVersion:       ver.version,
						CipherSuites:     ecdheCiphers,
						CurvePreferences: []CurveID{curve.id},
						Bugs: ProtocolBugs{
							TruncateKeyShare: true,
						},
					},
					flags:              flagInts("-curves", shimConfig.AllCurves),
					shouldFail:         true,
					expectedError:      ":BAD_ECPOINT:",
					expectedLocalError: badKeyShareLocalError,
				})

				testCases = append(testCases, testCase{
					testType: testType,
					name:     "CurveTest-Invalid-PadKeyShare-" + suffix,
					config: Config{
						MaxVersion:       ver.version,
						CipherSuites:     ecdheCiphers,
						CurvePreferences: []CurveID{curve.id},
						Bugs: ProtocolBugs{
							PadKeyShare: true,
						},
					},
					flags:              flagInts("-curves", shimConfig.AllCurves),
					shouldFail:         true,
					expectedError:      ":BAD_ECPOINT:",
					expectedLocalError: badKeyShareLocalError,
				})

				if isECDHGroup(curve.id) {
					testCases = append(testCases, testCase{
						testType: testType,
						name:     "CurveTest-Invalid-Compressed-" + suffix,
						config: Config{
							MaxVersion:       ver.version,
							CipherSuites:     ecdheCiphers,
							CurvePreferences: []CurveID{curve.id},
							Bugs: ProtocolBugs{
								SendCompressedCoordinates: true,
							},
						},
						flags:              flagInts("-curves", shimConfig.AllCurves),
						shouldFail:         true,
						expectedError:      ":BAD_ECPOINT:",
						expectedLocalError: badKeyShareLocalError,
					})
					testCases = append(testCases, testCase{
						testType: testType,
						name:     "CurveTest-Invalid-NotOnCurve-" + suffix,
						config: Config{
							MaxVersion:       ver.version,
							CipherSuites:     ecdheCiphers,
							CurvePreferences: []CurveID{curve.id},
							Bugs: ProtocolBugs{
								ECDHPointNotOnCurve: true,
							},
						},
						flags:              flagInts("-curves", shimConfig.AllCurves),
						shouldFail:         true,
						expectedError:      ":BAD_ECPOINT:",
						expectedLocalError: badKeyShareLocalError,
					})
				}

				if isX25519Group(curve.id) {

					testCases = append(testCases, testCase{
						testType: testType,
						name:     "CurveTest-SetX25519HighBit-" + suffix,
						config: Config{
							MaxVersion:       ver.version,
							CipherSuites:     ecdheCiphers,
							CurvePreferences: []CurveID{curve.id},
							Bugs: ProtocolBugs{
								SetX25519HighBit: true,
							},
						},
						flags: flagInts("-curves", shimConfig.AllCurves),
						expectations: connectionExpectations{
							curveID: curve.id,
						},
					})

					testCases = append(testCases, testCase{
						testType: testType,
						name:     "CurveTest-Invalid-LowOrderX25519Point-" + suffix,
						config: Config{
							MaxVersion:       ver.version,
							CipherSuites:     ecdheCiphers,
							CurvePreferences: []CurveID{curve.id},
							Bugs: ProtocolBugs{
								LowOrderX25519Point: true,
							},
						},
						flags:              flagInts("-curves", shimConfig.AllCurves),
						shouldFail:         true,
						expectedError:      ":BAD_ECPOINT:",
						expectedLocalError: badKeyShareLocalError,
					})
				}

				if curve.id == CurveX25519MLKEM768 && testType == serverTest {
					testCases = append(testCases, testCase{
						testType: testType,
						name:     "CurveTest-Invalid-MLKEMEncapKeyNotReduced-" + suffix,
						config: Config{
							MaxVersion:       ver.version,
							CipherSuites:     ecdheCiphers,
							CurvePreferences: []CurveID{curve.id},
							Bugs: ProtocolBugs{
								MLKEMEncapKeyNotReduced: true,
							},
						},
						flags:              flagInts("-curves", shimConfig.AllCurves),
						shouldFail:         true,
						expectedError:      ":BAD_ECPOINT:",
						expectedLocalError: badKeyShareLocalError,
					})
				}
			}
		}
	}

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "UnknownCurve",
		config: Config{
			MaxVersion:       VersionTLS12,
			CipherSuites:     []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			CurvePreferences: []CurveID{bogusCurve, CurveP256},
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "UnknownCurve-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{bogusCurve, CurveP256},
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoSupportedCurves",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			Bugs: ProtocolBugs{
				NoSupportedCurves: true,
			},
		},
		shouldFail:    true,
		expectedError: ":NO_SHARED_CIPHER:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoSupportedCurves-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				NoSupportedCurves: true,
			},
		},
		shouldFail:    true,
		expectedError: ":NO_SHARED_GROUP:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoCommonCurves",
		config: Config{
			MaxVersion: VersionTLS12,
			CipherSuites: []uint16{
				TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
				TLS_RSA_WITH_AES_128_GCM_SHA256,
			},
			CurvePreferences: []CurveID{CurveP224},
		},
		expectations: connectionExpectations{
			cipher: TLS_RSA_WITH_AES_128_GCM_SHA256,
		},
	})

	testCases = append(testCases, testCase{
		name: "BadECDHECurve",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			Bugs: ProtocolBugs{
				SendCurve: bogusCurve,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})
	testCases = append(testCases, testCase{
		name: "BadECDHECurve-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendCurve: bogusCurve,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "UnsupportedCurve",
		config: Config{
			MaxVersion:       VersionTLS12,
			CipherSuites:     []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
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

		name: "UnsupportedCurve-ServerHello-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				SendCurve: CurveP256,
			},
		},
		flags:         []string{"-curves", strconv.Itoa(int(CurveP384))},
		shouldFail:    true,
		expectedError: ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		name: "CurveID-Resume-Client",
		config: Config{
			MaxVersion:       VersionTLS12,
			CipherSuites:     []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			CurvePreferences: []CurveID{CurveX25519},
		},
		flags:         []string{"-expect-curve-id", strconv.Itoa(int(CurveX25519))},
		resumeSession: true,
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "CurveID-Resume-Server",
		config: Config{
			MaxVersion:       VersionTLS12,
			CipherSuites:     []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			CurvePreferences: []CurveID{CurveX25519},
		},
		flags:         []string{"-expect-curve-id", strconv.Itoa(int(CurveX25519))},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		name: "CurveID-Resume-Client-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveX25519},
		},
		resumeConfig: &Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveP256},
		},
		flags: []string{
			"-on-initial-expect-curve-id", strconv.Itoa(int(CurveX25519)),
			"-on-resume-expect-curve-id", strconv.Itoa(int(CurveP256)),
		},
		resumeSession: true,
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "CurveID-Resume-Server-TLS13",
		config: Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveX25519},
		},
		resumeConfig: &Config{
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveP256},
		},
		flags: []string{
			"-on-initial-expect-curve-id", strconv.Itoa(int(CurveX25519)),
			"-on-resume-expect-curve-id", strconv.Itoa(int(CurveP256)),
		},
		resumeSession: true,
	})

	testCases = append(testCases, testCase{
		name: "PointFormat-ServerHello-TLS12",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{pointFormatUncompressed},
			},
		},
	})
	testCases = append(testCases, testCase{
		name: "PointFormat-EncryptedExtensions-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{pointFormatUncompressed},
			},
		},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "SupportedCurves-ServerHello-TLS12",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendServerSupportedCurves: true,
			},
		},
	})
	testCases = append(testCases, testCase{
		name: "SupportedCurves-EncryptedExtensions-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				SendServerSupportedCurves: true,
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "PointFormat-Client-Tolerance",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{42, pointFormatUncompressed, 99, pointFormatCompressedPrime},
			},
		},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PointFormat-Server-Tolerance",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256},
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{42, pointFormatUncompressed, 99, pointFormatCompressedPrime},
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "PointFormat-Client-Missing",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256},
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{},
			},
		},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PointFormat-Server-Missing",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256},
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{},
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "PointFormat-Client-MissingUncompressed",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{pointFormatCompressedPrime},
			},
		},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PointFormat-Server-MissingUncompressed",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				SendSupportedPointFormats: []byte{pointFormatCompressedPrime},
			},
		},
		shouldFail:    true,
		expectedError: ":ERROR_PARSING_EXTENSION:",
	})

	for _, curve := range testCurves {
		if !isPqGroup(curve.id) {
			continue
		}

		testCases = append(testCases, testCase{
			name: "TLS12ClientShouldNotOffer-" + curve.name,
			config: Config{
				Bugs: ProtocolBugs{
					FailIfPostQuantumOffered: true,
				},
			},
			flags: []string{
				"-max-version", strconv.Itoa(VersionTLS12),
				"-curves", strconv.Itoa(int(curve.id)),
				"-curves", strconv.Itoa(int(CurveX25519)),
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "TLS12ServerShouldNotSelect-" + curve.name,
			flags: []string{
				"-max-version", strconv.Itoa(VersionTLS12),
				"-curves", strconv.Itoa(int(curve.id)),
				"-curves", strconv.Itoa(int(CurveX25519)),
			},
			expectations: connectionExpectations{
				curveID: CurveX25519,
			},
		})

		testCases = append(testCases, testCase{
			name: "ClientShouldNotAllowInTLS12-" + curve.name,
			config: Config{
				MaxVersion: VersionTLS12,
				Bugs: ProtocolBugs{
					SendCurve: curve.id,
				},
			},
			flags: []string{
				"-curves", strconv.Itoa(int(curve.id)),
				"-curves", strconv.Itoa(int(CurveX25519)),
			},
			shouldFail:         true,
			expectedError:      ":WRONG_CURVE:",
			expectedLocalError: "remote error: illegal parameter",
		})
	}

	testCases = append(testCases, testCase{
		name: "PostQuantumNotEnabledByDefaultInClients",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				FailIfPostQuantumOffered: true,
			},
		},
	})

	testCases = append(testCases, testCase{
		name: "NotJustMLKEMKeyShare",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectedKeyShares: []CurveID{CurveX25519MLKEM768, CurveX25519},
			},
		},
		flags: []string{
			"-curves", strconv.Itoa(int(CurveX25519MLKEM768)),
			"-curves", strconv.Itoa(int(CurveX25519)),
			"-expect-curve-id", strconv.Itoa(int(CurveX25519MLKEM768)),
		},
	})

	testCases = append(testCases, testCase{
		name: "MLKEMKeyShareIncludedSecond",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectedKeyShares: []CurveID{CurveX25519, CurveX25519MLKEM768},
			},
		},
		flags: []string{
			"-curves", strconv.Itoa(int(CurveX25519)),
			"-curves", strconv.Itoa(int(CurveX25519MLKEM768)),
			"-expect-curve-id", strconv.Itoa(int(CurveX25519)),
		},
	})

	testCases = append(testCases, testCase{
		name: "MLKEMKeyShareIncludedThird",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectedKeyShares: []CurveID{CurveX25519, CurveX25519MLKEM768},
			},
		},
		flags: []string{
			"-curves", strconv.Itoa(int(CurveX25519)),
			"-curves", strconv.Itoa(int(CurveP256)),
			"-curves", strconv.Itoa(int(CurveX25519MLKEM768)),
			"-expect-curve-id", strconv.Itoa(int(CurveX25519)),
		},
	})

	testCases = append(testCases, testCase{
		name: "JustConfiguringMLKEMWorks",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectedKeyShares: []CurveID{CurveX25519MLKEM768},
			},
		},
		flags: []string{
			"-curves", strconv.Itoa(int(CurveX25519MLKEM768)),
			"-expect-curve-id", strconv.Itoa(int(CurveX25519MLKEM768)),
		},
	})

	testCases = append(testCases, testCase{
		name: "BothMLKEMAndKyber",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				ExpectedKeyShares: []CurveID{CurveX25519MLKEM768},
			},
		},
		flags: []string{
			"-curves", strconv.Itoa(int(CurveX25519MLKEM768)),
			"-curves", strconv.Itoa(int(CurveX25519Kyber768)),
			"-expect-curve-id", strconv.Itoa(int(CurveX25519MLKEM768)),
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PostQuantumNotEnabledByDefaultForAServer",
		config: Config{
			MinVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveX25519MLKEM768, CurveX25519Kyber768, CurveX25519},
			DefaultCurves:    []CurveID{CurveX25519MLKEM768, CurveX25519Kyber768},
		},
		flags: []string{
			"-server-preference",
			"-expect-curve-id", strconv.Itoa(int(CurveX25519)),
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "CheckECDSACurve-TLS12",
		config: Config{
			MinVersion:       VersionTLS12,
			MaxVersion:       VersionTLS12,
			CurvePreferences: []CurveID{CurveP384},
		},
		shimCertificate: &ecdsaP256Certificate,
		shouldFail:      true,
		expectedError:   ":WRONG_CURVE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "CheckECDSACurve-PSK-TLS12",
		config: Config{
			MinVersion: VersionTLS12,
			MaxVersion: VersionTLS12,
			CipherSuites: []uint16{
				TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
				TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA,
			},
			CurvePreferences:     []CurveID{CurveP384},
			PreSharedKey:         []byte("12345"),
			PreSharedKeyIdentity: "luggage combo",
		},
		shimCertificate: &ecdsaP256Certificate,
		expectations: connectionExpectations{
			cipher: TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA,
		},
		flags: []string{
			"-psk", "12345",
			"-psk-identity", "luggage combo",
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "CheckECDSACurve-NotApplicable-TLS13",
		config: Config{
			MinVersion:       VersionTLS13,
			MaxVersion:       VersionTLS13,
			CurvePreferences: []CurveID{CurveP384},
		},
		shimCertificate: &ecdsaP256Certificate,
	})
}
