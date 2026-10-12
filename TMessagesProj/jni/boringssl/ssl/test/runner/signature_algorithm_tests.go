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

var testSignatureAlgorithms = []struct {
	name     string
	id       signatureAlgorithm
	baseCert *Credential

	curve CurveID
}{
	{"RSA_PKCS1_SHA1", signatureRSAPKCS1WithSHA1, &rsaCertificate, 0},
	{"RSA_PKCS1_SHA256", signatureRSAPKCS1WithSHA256, &rsaCertificate, 0},
	{"RSA_PKCS1_SHA256_LEGACY", signatureRSAPKCS1WithSHA256Legacy, &rsaCertificate, 0},
	{"RSA_PKCS1_SHA384", signatureRSAPKCS1WithSHA384, &rsaCertificate, 0},
	{"RSA_PKCS1_SHA512", signatureRSAPKCS1WithSHA512, &rsaCertificate, 0},
	{"ECDSA_SHA1", signatureECDSAWithSHA1, &ecdsaP256Certificate, CurveP256},

	{"ECDSA_P224_SHA256", signatureECDSAWithP256AndSHA256, &ecdsaP224Certificate, CurveP224},
	{"ECDSA_P256_SHA256", signatureECDSAWithP256AndSHA256, &ecdsaP256Certificate, CurveP256},
	{"ECDSA_P384_SHA384", signatureECDSAWithP384AndSHA384, &ecdsaP384Certificate, CurveP384},
	{"ECDSA_P521_SHA512", signatureECDSAWithP521AndSHA512, &ecdsaP521Certificate, CurveP521},
	{"RSA_PSS_SHA256", signatureRSAPSSWithSHA256, &rsaCertificate, 0},
	{"RSA_PSS_SHA384", signatureRSAPSSWithSHA384, &rsaCertificate, 0},
	{"RSA_PSS_SHA512", signatureRSAPSSWithSHA512, &rsaCertificate, 0},
	{"Ed25519", signatureEd25519, &ed25519Certificate, 0},

	{"RSA", 0, &rsaCertificate, 0},
	{"ECDSA", 0, &ecdsaP256Certificate, CurveP256},
}

const (
	fakeSigAlg1 signatureAlgorithm = 0x2a01
	fakeSigAlg2 signatureAlgorithm = 0xff01
)

func addSignatureAlgorithmTests() {

	signingCiphers := []uint16{
		TLS_AES_256_GCM_SHA384,
		TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
	}

	var allAlgorithms []signatureAlgorithm
	for _, alg := range testSignatureAlgorithms {
		if alg.id != 0 {
			allAlgorithms = append(allAlgorithms, alg.id)
		}
	}

	for _, alg := range testSignatureAlgorithms {

		cert := alg.baseCert
		if alg.id != 0 {
			cert = cert.WithSignatureAlgorithms(alg.id)
		}

		for _, ver := range tlsVersions {
			if (ver.version < VersionTLS12) != (alg.id == 0) {
				continue
			}

			suffix := "-" + alg.name + "-" + ver.name
			for _, signTestType := range []testType{clientTest, serverTest} {
				signPrefix := "Client-"
				verifyPrefix := "Server-"
				verifyTestType := serverTest
				if signTestType == serverTest {
					verifyTestType = clientTest
					signPrefix, verifyPrefix = verifyPrefix, signPrefix
				}

				var shouldFail bool
				isTLS12PKCS1 := hasComponent(alg.name, "PKCS1") && !hasComponent(alg.name, "LEGACY")
				isTLS13PKCS1 := hasComponent(alg.name, "PKCS1") && hasComponent(alg.name, "LEGACY")

				if ver.version >= VersionTLS13 && (alg.curve == CurveP224 || alg.id == signatureECDSAWithSHA1 || isTLS12PKCS1) {
					shouldFail = true
				}

				if (ver.version < VersionTLS13 || signTestType == serverTest) && isTLS13PKCS1 {
					shouldFail = true
				}

				signDefault := !shouldFail
				if isTLS13PKCS1 {
					signDefault = false
				}

				verifyDefault := !shouldFail
				if alg.id == signatureECDSAWithSHA1 || alg.id == signatureECDSAWithP521AndSHA512 || alg.id == signatureEd25519 || isTLS13PKCS1 {
					verifyDefault = false
				}

				var curveFlags []string
				var runnerCurves []CurveID
				if alg.curve != 0 && ver.version <= VersionTLS12 {

					curveFlags = flagInts("-curves", []int{int(CurveX25519), int(alg.curve)})
					runnerCurves = []CurveID{CurveX25519, alg.curve}
				}

				signError := func(shouldFail bool) string {
					if !shouldFail {
						return ""
					}

					if ver.version <= VersionTLS12 && signTestType == serverTest {
						return ":NO_SHARED_CIPHER:"
					}
					return ":NO_COMMON_SIGNATURE_ALGORITHMS:"
				}
				signLocalError := func(shouldFail bool) string {
					if !shouldFail {
						return ""
					}

					return "remote error: handshake failure"
				}
				verifyError := func(shouldFail bool) string {
					if !shouldFail {
						return ""
					}

					return ":WRONG_SIGNATURE_TYPE:"
				}
				verifyLocalError := func(shouldFail bool) string {
					if !shouldFail {
						return ""
					}

					return "remote error: illegal parameter"
				}

				signTest := testCase{
					testType: signTestType,
					name:     signPrefix + "Sign" + suffix,
					config: Config{
						MaxVersion:       ver.version,
						CurvePreferences: runnerCurves,
						VerifySignatureAlgorithms: []signatureAlgorithm{
							fakeSigAlg1,
							alg.id,
							fakeSigAlg2,
						},
					},
					shimCertificate:    cert,
					flags:              curveFlags,
					shouldFail:         shouldFail,
					expectedError:      signError(shouldFail),
					expectedLocalError: signLocalError(shouldFail),
					expectations: connectionExpectations{
						peerSignatureAlgorithm: alg.id,
					},
				}

				signDefaultTest := testCase{
					testType: signTestType,
					name:     signPrefix + "SignDefault" + suffix,
					config: Config{
						MaxVersion:       ver.version,
						CurvePreferences: runnerCurves,
						VerifySignatureAlgorithms: []signatureAlgorithm{
							fakeSigAlg1,
							alg.id,
							fakeSigAlg2,
						},
					},

					shimCertificate:    alg.baseCert,
					flags:              curveFlags,
					shouldFail:         !signDefault,
					expectedError:      signError(!signDefault),
					expectedLocalError: signLocalError(!signDefault),
					expectations: connectionExpectations{
						peerSignatureAlgorithm: alg.id,
					},
				}

				negotiateTest := testCase{
					testType: signTestType,
					name:     signPrefix + "Sign-Negotiate" + suffix,
					config: Config{
						MaxVersion:                ver.version,
						CurvePreferences:          runnerCurves,
						VerifySignatureAlgorithms: allAlgorithms,
					},
					shimCertificate: cert,
					flags:           curveFlags,
					expectations: connectionExpectations{
						peerSignatureAlgorithm: alg.id,
					},
				}

				if signTestType == serverTest {

					signTest.config.CipherSuites = signingCiphers
					signDefaultTest.config.CipherSuites = signingCiphers
					negotiateTest.config.CipherSuites = signingCiphers
				} else {

					signTest.config.ClientAuth = RequireAnyClientCert
					signDefaultTest.config.ClientAuth = RequireAnyClientCert
					negotiateTest.config.ClientAuth = RequireAnyClientCert
				}
				testCases = append(testCases, signTest, signDefaultTest)
				if ver.version >= VersionTLS12 && !shouldFail {
					testCases = append(testCases, negotiateTest)
				}

				verifyTest := testCase{
					testType: verifyTestType,
					name:     verifyPrefix + "Verify" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Credential: cert,
						Bugs: ProtocolBugs{
							SkipECDSACurveCheck:          shouldFail,
							IgnoreSignatureVersionChecks: shouldFail,

							IgnorePeerSignatureAlgorithmPreferences: shouldFail,
						},
					},
					flags: curveFlags,

					resumeSession:      !shouldFail,
					shouldFail:         shouldFail,
					expectedError:      verifyError(shouldFail),
					expectedLocalError: verifyLocalError(shouldFail),
				}
				if alg.id != 0 {
					verifyTest.flags = append(verifyTest.flags, "-expect-peer-signature-algorithm", strconv.Itoa(int(alg.id)))

					verifyTest.flags = append(verifyTest.flags, "-verify-prefs", strconv.Itoa(int(alg.id)))
				}

				defaultTest := testCase{
					testType: verifyTestType,
					name:     verifyPrefix + "VerifyDefault" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Credential: cert,
						Bugs: ProtocolBugs{
							SkipECDSACurveCheck:          !verifyDefault,
							IgnoreSignatureVersionChecks: !verifyDefault,

							IgnorePeerSignatureAlgorithmPreferences: !verifyDefault,
						},
					},
					flags: append(
						[]string{"-expect-peer-signature-algorithm", strconv.Itoa(int(alg.id))},
						curveFlags...,
					),

					resumeSession:      verifyDefault,
					shouldFail:         !verifyDefault,
					expectedError:      verifyError(!verifyDefault),
					expectedLocalError: verifyLocalError(!verifyDefault),
				}

				invalidTest := testCase{
					testType: verifyTestType,
					name:     verifyPrefix + "InvalidSignature" + suffix,
					config: Config{
						MaxVersion: ver.version,
						Credential: cert,
						Bugs: ProtocolBugs{
							InvalidSignature: true,
						},
					},
					flags:         curveFlags,
					shouldFail:    true,
					expectedError: ":BAD_SIGNATURE:",
				}
				if alg.id != 0 {

					invalidTest.flags = append(invalidTest.flags, "-verify-prefs", strconv.Itoa(int(alg.id)))
				}

				if verifyTestType == serverTest {

					verifyTest.flags = append(verifyTest.flags, "-require-any-client-certificate")
					defaultTest.flags = append(defaultTest.flags, "-require-any-client-certificate")
					invalidTest.flags = append(invalidTest.flags, "-require-any-client-certificate")
				} else {

					verifyTest.config.CipherSuites = signingCiphers
					defaultTest.config.CipherSuites = signingCiphers
					invalidTest.config.CipherSuites = signingCiphers
				}
				testCases = append(testCases, verifyTest, defaultTest)
				if !shouldFail {
					testCases = append(testCases, invalidTest)
				}
			}
		}
	}

	for _, ver := range tlsVersions {
		if ver.version < VersionTLS12 {
			continue
		}
		testCases = append(testCases, testCase{
			name: "ClientAuth-PeerVerifyPrefs-" + ver.name,
			config: Config{
				MaxVersion: ver.version,
				ClientAuth: RequireAnyClientCert,
				VerifySignatureAlgorithms: []signatureAlgorithm{
					signatureRSAPSSWithSHA256,
					signatureEd25519,
					signatureECDSAWithP256AndSHA256,
				},
			},
			shimCertificate: &rsaCertificate,
			flags: []string{
				"-expect-peer-verify-pref", strconv.Itoa(int(signatureRSAPSSWithSHA256)),
				"-expect-peer-verify-pref", strconv.Itoa(int(signatureEd25519)),
				"-expect-peer-verify-pref", strconv.Itoa(int(signatureECDSAWithP256AndSHA256)),
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			name:     "ServerAuth-PeerVerifyPrefs-" + ver.name,
			config: Config{
				MaxVersion: ver.version,
				VerifySignatureAlgorithms: []signatureAlgorithm{
					signatureRSAPSSWithSHA256,
					signatureEd25519,
					signatureECDSAWithP256AndSHA256,
				},
			},
			shimCertificate: &rsaCertificate,
			flags: []string{
				"-expect-peer-verify-pref", strconv.Itoa(int(signatureRSAPSSWithSHA256)),
				"-expect-peer-verify-pref", strconv.Itoa(int(signatureEd25519)),
				"-expect-peer-verify-pref", strconv.Itoa(int(signatureECDSAWithP256AndSHA256)),
			},
		})

	}

	testCases = append(testCases, testCase{
		name: "ClientAuth-SignatureType",
		config: Config{
			ClientAuth: RequireAnyClientCert,
			MaxVersion: VersionTLS12,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithP521AndSHA512,
				signatureRSAPKCS1WithSHA384,
				signatureECDSAWithSHA1,
			},
		},
		shimCertificate: &rsaCertificate,
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPKCS1WithSHA384,
		},
	})

	testCases = append(testCases, testCase{
		name: "ClientAuth-SignatureType-TLS13",
		config: Config{
			ClientAuth: RequireAnyClientCert,
			MaxVersion: VersionTLS13,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithP521AndSHA512,
				signatureRSAPKCS1WithSHA384,
				signatureRSAPSSWithSHA384,
				signatureECDSAWithSHA1,
			},
		},
		shimCertificate: &rsaCertificate,
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPSSWithSHA384,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ServerAuth-SignatureType",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithP521AndSHA512,
				signatureRSAPKCS1WithSHA384,
				signatureECDSAWithSHA1,
			},
		},
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPKCS1WithSHA384,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ServerAuth-SignatureType-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithP521AndSHA512,
				signatureRSAPKCS1WithSHA384,
				signatureRSAPSSWithSHA384,
				signatureECDSAWithSHA1,
			},
		},
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPSSWithSHA384,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Verify-ClientAuth-SignatureType",
		config: Config{
			MaxVersion: VersionTLS12,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithSHA256),
			Bugs: ProtocolBugs{
				SendSignatureAlgorithm: signatureECDSAWithP256AndSHA256,
			},
		},
		flags: []string{
			"-require-any-client-certificate",
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "Verify-ClientAuth-SignatureType-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
			Bugs: ProtocolBugs{
				SendSignatureAlgorithm: signatureECDSAWithP256AndSHA256,
			},
		},
		flags: []string{
			"-require-any-client-certificate",
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "Verify-ServerAuth-SignatureType",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			Credential:   rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithSHA256),
			Bugs: ProtocolBugs{
				SendSignatureAlgorithm: signatureECDSAWithP256AndSHA256,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "Verify-ServerAuth-SignatureType-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
			Bugs: ProtocolBugs{
				SendSignatureAlgorithm: signatureECDSAWithP256AndSHA256,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ServerAuth-SHA1-Fallback-RSA",
		config: Config{
			MaxVersion: VersionTLS12,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA1,
			},
			Bugs: ProtocolBugs{
				NoSignatureAlgorithms: true,
			},
		},
		shimCertificate: &rsaCertificate,
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ServerAuth-SHA1-Fallback-ECDSA",
		config: Config{
			MaxVersion: VersionTLS12,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithSHA1,
			},
			Bugs: ProtocolBugs{
				NoSignatureAlgorithms: true,
			},
		},
		shimCertificate: &ecdsaP256Certificate,
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ServerAuth-NoFallback-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA1,
			},
			Bugs: ProtocolBugs{
				NoSignatureAlgorithms: true,
			},
		},
		shouldFail:    true,
		expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
	})

	testCases = append(testCases, testCase{
		name: "ClientAuth-NoFallback-RSA",
		config: Config{
			MaxVersion: VersionTLS12,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA1,
			},
			Bugs: ProtocolBugs{
				NoSignatureAlgorithms: true,
			},
		},
		shimCertificate:    &rsaCertificate,
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: error decoding message",
	})

	testCases = append(testCases, testCase{
		name: "ClientAuth-NoFallback-ECDSA",
		config: Config{
			MaxVersion: VersionTLS12,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithSHA1,
			},
			Bugs: ProtocolBugs{
				NoSignatureAlgorithms: true,
			},
		},
		shimCertificate:    &ecdsaP256Certificate,
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: error decoding message",
	})

	testCases = append(testCases, testCase{
		name: "ClientAuth-NoFallback-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA1,
			},
			Bugs: ProtocolBugs{
				NoSignatureAlgorithms: true,
			},
		},
		shimCertificate:    &rsaCertificate,
		shouldFail:         true,
		expectedError:      ":DECODE_ERROR:",
		expectedLocalError: "remote error: error decoding message",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ClientAuth-Enforced",
		config: Config{
			MaxVersion: VersionTLS12,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithMD5),
			Bugs: ProtocolBugs{
				IgnorePeerSignatureAlgorithmPreferences: true,
			},
		},
		flags:         []string{"-require-any-client-certificate"},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "ServerAuth-Enforced",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			Credential:   rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithMD5),
			Bugs: ProtocolBugs{
				IgnorePeerSignatureAlgorithmPreferences: true,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ClientAuth-Enforced-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithMD5),
			Bugs: ProtocolBugs{
				IgnorePeerSignatureAlgorithmPreferences: true,
				IgnoreSignatureVersionChecks:            true,
			},
		},
		flags:         []string{"-require-any-client-certificate"},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "ServerAuth-Enforced-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithMD5),
			Bugs: ProtocolBugs{
				IgnorePeerSignatureAlgorithmPreferences: true,
				IgnoreSignatureVersionChecks:            true,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "NoCommonAlgorithms",
		config: Config{
			MaxVersion: VersionTLS12,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA512,
				signatureRSAPKCS1WithSHA1,
			},
		},
		shimCertificate: rsaCertificate.WithSignatureAlgorithms(signatureRSAPKCS1WithSHA256),
		shouldFail:      true,
		expectedError:   ":NO_COMMON_SIGNATURE_ALGORITHMS:",
	})
	testCases = append(testCases, testCase{
		name: "NoCommonAlgorithms-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPSSWithSHA512,
				signatureRSAPSSWithSHA384,
			},
		},
		shimCertificate: rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
		shouldFail:      true,
		expectedError:   ":NO_COMMON_SIGNATURE_ALGORITHMS:",
	})
	testCases = append(testCases, testCase{
		name: "Agree-Digest-SHA256",
		config: Config{
			MaxVersion: VersionTLS12,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA1,
				signatureRSAPKCS1WithSHA256,
			},
		},
		shimCertificate: rsaCertificate.WithSignatureAlgorithms(
			signatureRSAPKCS1WithSHA256,
			signatureRSAPKCS1WithSHA1,
		),
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPKCS1WithSHA256,
		},
	})
	testCases = append(testCases, testCase{
		name: "Agree-Digest-SHA1",
		config: Config{
			MaxVersion: VersionTLS12,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA1,
			},
		},
		shimCertificate: rsaCertificate.WithSignatureAlgorithms(
			signatureRSAPKCS1WithSHA512,
			signatureRSAPKCS1WithSHA256,
			signatureRSAPKCS1WithSHA1,
		),
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPKCS1WithSHA1,
		},
	})
	testCases = append(testCases, testCase{
		name: "Agree-Digest-Default",
		config: Config{
			MaxVersion: VersionTLS12,
			ClientAuth: RequireAnyClientCert,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA256,
				signatureECDSAWithP256AndSHA256,
				signatureRSAPKCS1WithSHA1,
				signatureECDSAWithSHA1,
			},
		},
		shimCertificate: &rsaCertificate,
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPKCS1WithSHA256,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "FilterExtraAlgorithms",
		config: Config{
			MaxVersion: VersionTLS12,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPKCS1WithSHA256,
			},
		},
		shimCertificate: rsaCertificate.WithSignatureAlgorithms(
			signatureECDSAWithP256AndSHA256,
			signatureRSAPKCS1WithSHA256,
		),
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureRSAPKCS1WithSHA256,
		},
	})

	testCases = append(testCases, testCase{
		name: "CheckLeafCurve",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256},
			Credential:   &ecdsaP256Certificate,
		},
		flags:         []string{"-curves", strconv.Itoa(int(CurveP384))},
		shouldFail:    true,
		expectedError: ":BAD_ECC_CERT:",
	})

	testCases = append(testCases, testCase{
		name: "CheckLeafCurve-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: &ecdsaP256Certificate,
		},
		flags: []string{"-curves", strconv.Itoa(int(CurveP384))},
	})

	testCases = append(testCases, testCase{
		name: "ECDSACurveMismatch-Verify-TLS12",
		config: Config{
			MaxVersion:   VersionTLS12,
			CipherSuites: []uint16{TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256},
			Credential:   ecdsaP256Certificate.WithSignatureAlgorithms(signatureECDSAWithP384AndSHA384),
		},
	})

	testCases = append(testCases, testCase{
		name: "ECDSACurveMismatch-Verify-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			Credential: ecdsaP256Certificate.WithSignatureAlgorithms(signatureECDSAWithP384AndSHA384),
			Bugs: ProtocolBugs{
				SkipECDSACurveCheck: true,
			},
		},
		shouldFail:    true,
		expectedError: ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "ECDSACurveMismatch-Sign-TLS13",
		config: Config{
			MaxVersion: VersionTLS13,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithP384AndSHA384,
				signatureECDSAWithP256AndSHA256,
			},
		},
		shimCertificate: &ecdsaP256Certificate,
		expectations: connectionExpectations{
			peerSignatureAlgorithm: signatureECDSAWithP256AndSHA256,
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "RSA-PSS-Large",
		config: Config{
			MaxVersion: VersionTLS13,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPSSWithSHA512,
			},
		},
		shimCertificate: &rsa1024Certificate,
		shouldFail:      true,
		expectedError:   ":NO_COMMON_SIGNATURE_ALGORITHMS:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "RSA-PSS-Default-Verify",
		config: Config{
			MaxVersion: VersionTLS12,
			Credential: rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
		},
		flags: []string{"-max-version", strconv.Itoa(VersionTLS12)},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "RSA-PSS-Default-Sign",
		config: Config{
			MaxVersion: VersionTLS12,
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureRSAPSSWithSHA256,
			},
		},
		flags: []string{"-max-version", strconv.Itoa(VersionTLS12)},
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "NoEd25519-TLS11-ServerAuth-Verify",
		config: Config{
			MaxVersion: VersionTLS11,
			Credential: &ed25519Certificate,
			Bugs: ProtocolBugs{

				SigningAlgorithmForLegacyVersions: signatureEd25519,
			},
		},
		flags:         []string{"-verify-prefs", strconv.Itoa(int(signatureEd25519))},
		shouldFail:    true,
		expectedError: ":PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoEd25519-TLS11-ServerAuth-Sign",
		config: Config{
			MaxVersion: VersionTLS11,
		},
		shimCertificate: &ed25519Certificate,
		shouldFail:      true,
		expectedError:   ":NO_SHARED_CIPHER:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoEd25519-TLS11-ClientAuth-Verify",
		config: Config{
			MaxVersion: VersionTLS11,
			Credential: &ed25519Certificate,
			Bugs: ProtocolBugs{

				SigningAlgorithmForLegacyVersions: signatureEd25519,
			},
		},
		flags: []string{
			"-verify-prefs", strconv.Itoa(int(signatureEd25519)),
			"-require-any-client-certificate",
		},
		shouldFail:    true,
		expectedError: ":PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE:",
	})
	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "NoEd25519-TLS11-ClientAuth-Sign",
		config: Config{
			MaxVersion: VersionTLS11,
			ClientAuth: RequireAnyClientCert,
		},
		shimCertificate: &ed25519Certificate,
		shouldFail:      true,
		expectedError:   ":NO_COMMON_SIGNATURE_ALGORITHMS:",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "Ed25519DefaultDisable-NoAdvertise",
		config: Config{
			Credential: &ed25519Certificate,
		},
		shouldFail:         true,
		expectedLocalError: "tls: no common signature algorithms",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "Ed25519DefaultDisable-NoAccept",
		config: Config{
			Credential: &ed25519Certificate,
			Bugs: ProtocolBugs{
				IgnorePeerSignatureAlgorithmPreferences: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: illegal parameter",
		expectedError:      ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "VerifyPreferences-Advertised",
		config: Config{
			Credential: rsaCertificate.WithSignatureAlgorithms(
				signatureRSAPSSWithSHA256,
				signatureRSAPSSWithSHA384,
				signatureRSAPSSWithSHA512,
			),
		},
		flags: []string{
			"-verify-prefs", strconv.Itoa(int(signatureRSAPSSWithSHA384)),
			"-expect-peer-signature-algorithm", strconv.Itoa(int(signatureRSAPSSWithSHA384)),
		},
	})

	testCases = append(testCases, testCase{
		name: "VerifyPreferences-NoCommonAlgorithms",
		config: Config{
			Credential: rsaCertificate.WithSignatureAlgorithms(
				signatureRSAPSSWithSHA256,
				signatureRSAPSSWithSHA512,
			),
		},
		flags: []string{
			"-verify-prefs", strconv.Itoa(int(signatureRSAPSSWithSHA384)),
		},
		shouldFail:         true,
		expectedLocalError: "tls: no common signature algorithms",
	})

	testCases = append(testCases, testCase{
		name: "VerifyPreferences-Enforced",
		config: Config{
			Credential: rsaCertificate.WithSignatureAlgorithms(
				signatureRSAPSSWithSHA256,
				signatureRSAPSSWithSHA512,
			),
			Bugs: ProtocolBugs{
				IgnorePeerSignatureAlgorithmPreferences: true,
			},
		},
		flags: []string{
			"-verify-prefs", strconv.Itoa(int(signatureRSAPSSWithSHA384)),
		},
		shouldFail:         true,
		expectedLocalError: "remote error: illegal parameter",
		expectedError:      ":WRONG_SIGNATURE_TYPE:",
	})

	testCases = append(testCases, testCase{
		name: "VerifyPreferences-Ed25519",
		config: Config{
			Credential: &ed25519Certificate,
		},
		flags: []string{
			"-verify-prefs", strconv.Itoa(int(signatureEd25519)),
		},
	})

	for _, testType := range []testType{clientTest, serverTest} {
		for _, ver := range tlsVersions {
			if ver.version < VersionTLS12 {
				continue
			}

			prefix := "Client-" + ver.name + "-"
			noCommonAlgorithmsError := ":NO_COMMON_SIGNATURE_ALGORITHMS:"
			if testType == serverTest {
				prefix = "Server-" + ver.name + "-"

				if ver.version <= VersionTLS12 {
					noCommonAlgorithmsError = ":NO_SHARED_CIPHER:"
				}
			}

			testCases = append(testCases, testCase{
				testType: testType,
				name:     prefix + "NoSign-RSA_PKCS1_MD5_SHA1",
				config: Config{
					MaxVersion:                ver.version,
					CipherSuites:              signingCiphers,
					ClientAuth:                RequireAnyClientCert,
					VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPKCS1WithMD5AndSHA1},
				},
				shimCertificate: rsaCertificate.WithSignatureAlgorithms(
					signatureRSAPKCS1WithMD5AndSHA1,

					signatureRSAPKCS1WithSHA256,
				),
				shouldFail:    true,
				expectedError: noCommonAlgorithmsError,
			})

			testCases = append(testCases, testCase{
				testType: testType,
				name:     prefix + "NoVerify-RSA_PKCS1_MD5_SHA1",
				config: Config{
					MaxVersion: ver.version,
					Credential: &rsaCertificate,
					Bugs: ProtocolBugs{
						IgnorePeerSignatureAlgorithmPreferences: true,
						AlwaysSignAsLegacyVersion:               true,
						SendSignatureAlgorithm:                  signatureRSAPKCS1WithMD5AndSHA1,
					},
				},
				shimCertificate: &rsaCertificate,
				flags: []string{
					"-verify-prefs", strconv.Itoa(int(signatureRSAPKCS1WithMD5AndSHA1)),

					"-verify-prefs", strconv.Itoa(int(signatureRSAPKCS1WithSHA256)),
					"-require-any-client-certificate",
				},
				shouldFail:    true,
				expectedError: ":WRONG_SIGNATURE_TYPE:",
			})
		}
	}

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "NoCommonSignatureAlgorithms-TLS12-Fallback",
		config: Config{
			MaxVersion: VersionTLS12,
			CipherSuites: []uint16{
				TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
				TLS_RSA_WITH_AES_128_GCM_SHA256,
			},
			VerifySignatureAlgorithms: []signatureAlgorithm{
				signatureECDSAWithP256AndSHA256,
			},
		},
		expectations: connectionExpectations{
			cipher: TLS_RSA_WITH_AES_128_GCM_SHA256,
		},
	})
}

func addBadECDSASignatureTests() {
	for badR := BadValue(1); badR < NumBadValues; badR++ {
		for badS := BadValue(1); badS < NumBadValues; badS++ {
			testCases = append(testCases, testCase{
				name: fmt.Sprintf("BadECDSA-%d-%d", badR, badS),
				config: Config{
					MaxVersion:   VersionTLS12,
					CipherSuites: []uint16{TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256},
					Credential:   &ecdsaP256Certificate,
					Bugs: ProtocolBugs{
						BadECDSAR: badR,
						BadECDSAS: badS,
					},
				},
				shouldFail:    true,
				expectedError: ":BAD_SIGNATURE:",
			})
			testCases = append(testCases, testCase{
				name: fmt.Sprintf("BadECDSA-%d-%d-TLS13", badR, badS),
				config: Config{
					MaxVersion: VersionTLS13,
					Credential: &ecdsaP256Certificate,
					Bugs: ProtocolBugs{
						BadECDSAR: badR,
						BadECDSAS: badS,
					},
				},
				shouldFail:    true,
				expectedError: ":BAD_SIGNATURE:",
			})
		}
	}
}
