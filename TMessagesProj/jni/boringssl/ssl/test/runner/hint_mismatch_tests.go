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

func addHintMismatchTests() {

	for _, protocol := range []protocol{tls, quic} {

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-SignatureInput",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				NextProtos: []string{"foo", "bar"},
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-shim-select-alpn", "foo",
				"-on-handshaker-select-alpn", "bar",
			},
			expectations: connectionExpectations{
				nextProto:     "foo",
				nextProtoType: alpn,
			},
		})

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-KeyShare",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,

				DefaultCurves: []CurveID{CurveX25519, CurveP256},
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-shim-curves", strconv.Itoa(int(CurveX25519)),
				"-on-handshaker-curves", strconv.Itoa(int(CurveP256)),
			},
			expectations: connectionExpectations{
				curveID: CurveX25519,
			},
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-ECDHE-Group",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion:    VersionTLS12,
					MaxVersion:    VersionTLS12,
					DefaultCurves: []CurveID{CurveX25519, CurveP256},
				},
				flags: []string{
					"-allow-hint-mismatch",
					"-on-shim-curves", strconv.Itoa(int(CurveX25519)),
					"-on-handshaker-curves", strconv.Itoa(int(CurveP256)),
				},
				expectations: connectionExpectations{
					curveID: CurveX25519,
				},
			})
		}

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-HandshakerHelloRetryRequest",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion:    VersionTLS13,
				MaxVersion:    VersionTLS13,
				DefaultCurves: []CurveID{CurveX25519},
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-shim-curves", strconv.Itoa(int(CurveX25519)),
				"-on-handshaker-curves", strconv.Itoa(int(CurveP256)),
			},
			expectations: connectionExpectations{
				curveID: CurveX25519,
			},
		})

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-ShimHelloRetryRequest",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion:    VersionTLS13,
				MaxVersion:    VersionTLS13,
				DefaultCurves: []CurveID{CurveX25519},
			},
			flags: []string{
				"-on-shim-curves", strconv.Itoa(int(CurveP256)),
				"-on-handshaker-curves", strconv.Itoa(int(CurveX25519)),
			},
			expectations: connectionExpectations{
				curveID: CurveP256,
			},
		})

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-SignatureAlgorithm-TLS13",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				VerifySignatureAlgorithms: []signatureAlgorithm{
					signatureRSAPSSWithSHA256,
					signatureRSAPSSWithSHA384,
				},
			},
			shimCertificate:       rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
			handshakerCertificate: rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
			flags:                 []string{"-allow-hint-mismatch"},
			expectations: connectionExpectations{
				peerSignatureAlgorithm: signatureRSAPSSWithSHA256,
			},
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-SignatureAlgorithm-TLS12",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
					VerifySignatureAlgorithms: []signatureAlgorithm{
						signatureRSAPSSWithSHA256,
						signatureRSAPSSWithSHA384,
					},
				},
				shimCertificate:       rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
				handshakerCertificate: rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
				flags:                 []string{"-allow-hint-mismatch"},
				expectations: connectionExpectations{
					peerSignatureAlgorithm: signatureRSAPSSWithSHA256,
				},
			})
		}

		ecdsaP256Certificate2 := generateSingleCertChain(nil, &channelIDKey)
		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-Certificate-TLS13",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
			},
			shimCertificate:       &ecdsaP256Certificate,
			handshakerCertificate: &ecdsaP256Certificate2,
			flags:                 []string{"-allow-hint-mismatch"},
			expectations: connectionExpectations{
				peerCertificate: &ecdsaP256Certificate,
			},
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-Certificate-TLS12",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
				},
				shimCertificate:       &ecdsaP256Certificate,
				handshakerCertificate: &ecdsaP256Certificate2,
				flags:                 []string{"-allow-hint-mismatch"},
				expectations: connectionExpectations{
					peerCertificate: &ecdsaP256Certificate,
				},
			})
		}

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-NoTickets1-TLS13",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
			},
			flags: []string{
				"-on-resume-allow-hint-mismatch",
				"-on-shim-on-resume-no-ticket",
			},
			resumeSession:        true,
			expectResumeRejected: true,
		})
		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-NoTickets2-TLS13",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
			},
			flags: []string{
				"-on-resume-allow-hint-mismatch",
				"-on-handshaker-on-resume-no-ticket",
			},
			resumeSession: true,
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-NoTickets1-TLS12",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
				},
				flags: []string{
					"-on-resume-allow-hint-mismatch",
					"-on-shim-on-resume-no-ticket",
				},
				resumeSession:        true,
				expectResumeRejected: true,
			})
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-NoTickets2-TLS12",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
				},
				flags: []string{
					"-on-resume-allow-hint-mismatch",
					"-on-handshaker-on-resume-no-ticket",
				},
				resumeSession: true,
			})
		}

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-CertificateRequest",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Credential: &rsaCertificate,
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-shim-require-any-client-certificate",
			},
		})

		if protocol != quic {
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-Version1",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS13,
				},
				flags: []string{
					"-allow-hint-mismatch",
					"-on-shim-max-version", strconv.Itoa(VersionTLS12),
					"-on-handshaker-max-version", strconv.Itoa(VersionTLS13),
				},
				expectations: connectionExpectations{
					version: VersionTLS12,
				},
			})
			testCases = append(testCases, testCase{
				name:               protocol.String() + "-HintMismatch-Version2",
				testType:           serverTest,
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS13,
				},
				flags: []string{
					"-allow-hint-mismatch",
					"-on-shim-max-version", strconv.Itoa(VersionTLS13),
					"-on-handshaker-max-version", strconv.Itoa(VersionTLS12),
				},
				expectations: connectionExpectations{
					version: VersionTLS13,
				},
			})
		}

		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-CertificateCompression-ShimOnly",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				CertCompressionAlgs: map[uint16]CertCompressionAlg{
					shrinkingCompressionAlgID: shrinkingCompression,
				},
				Bugs: ProtocolBugs{
					ExpectedCompressedCert: shrinkingCompressionAlgID,
				},
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-shim-install-cert-compression-algs",
			},
		})
		testCases = append(testCases, testCase{
			name:               protocol.String() + "-HintMismatch-CertificateCompression-HandshakerOnly",
			testType:           serverTest,
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				CertCompressionAlgs: map[uint16]CertCompressionAlg{
					shrinkingCompressionAlgID: shrinkingCompression,
				},
				Bugs: ProtocolBugs{
					ExpectUncompressedCert: true,
				},
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-handshaker-install-cert-compression-algs",
			},
		})
		testCases = append(testCases, testCase{
			testType:           serverTest,
			name:               protocol.String() + "-HintMismatch-CertificateCompression-AlgorithmMismatch",
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				CertCompressionAlgs: map[uint16]CertCompressionAlg{
					shrinkingCompressionAlgID: shrinkingCompression,
					expandingCompressionAlgID: expandingCompression,
				},
				Bugs: ProtocolBugs{

					ExpectedCompressedCert: shrinkingCompressionAlgID,
				},
			},
			flags: []string{
				"-allow-hint-mismatch",
				"-on-shim-install-one-cert-compression-alg", strconv.Itoa(shrinkingCompressionAlgID),
				"-on-handshaker-install-one-cert-compression-alg", strconv.Itoa(expandingCompressionAlgID),
			},
		})
		testCases = append(testCases, testCase{
			testType:           serverTest,
			name:               protocol.String() + "-HintMismatch-CertificateCompression-InputMismatch",
			protocol:           protocol,
			skipSplitHandshake: true,
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				CertCompressionAlgs: map[uint16]CertCompressionAlg{
					shrinkingCompressionAlgID: shrinkingCompression,
				},
				Bugs: ProtocolBugs{
					ExpectedCompressedCert: shrinkingCompressionAlgID,
				},
			},

			shimCertificate:       rsaCertificate.WithOCSP(testOCSPResponse),
			handshakerCertificate: rsaCertificate.WithOCSP(testOCSPResponse2),
			flags: []string{
				"-allow-hint-mismatch",
				"-install-cert-compression-algs",
			},
			expectations: connectionExpectations{

				peerCertificate: rsaCertificate.WithOCSP(testOCSPResponse),
			},
		})

		if protocol != quic {
			testCases = append(testCases, testCase{
				testType:           serverTest,
				name:               protocol.String() + "-HintMismatch-CipherMismatch1",
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
				},
				flags: []string{
					"-allow-hint-mismatch",
					"-on-shim-cipher", "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
					"-on-handshaker-cipher", "TLS_RSA_WITH_AES_128_GCM_SHA256",
				},
				expectations: connectionExpectations{
					cipher: TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
				},
			})
			testCases = append(testCases, testCase{
				testType:           serverTest,
				name:               protocol.String() + "-HintMismatch-CipherMismatch2",
				protocol:           protocol,
				skipSplitHandshake: true,
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
				},
				flags: []string{

					"-on-shim-cipher", "TLS_RSA_WITH_AES_128_GCM_SHA256",
					"-on-handshaker-cipher", "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
				},
				expectations: connectionExpectations{
					cipher: TLS_RSA_WITH_AES_128_GCM_SHA256,
				},
			})
		}
	}
}
