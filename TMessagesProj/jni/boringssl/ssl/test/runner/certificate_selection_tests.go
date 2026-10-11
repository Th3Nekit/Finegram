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

func canBeShimCertificate(c *Credential) bool {

	return c.Type == CredentialTypeX509 && !c.MustMatchIssuer && c.TrustAnchorID == nil
}

func addCertificateSelectionTests() {

	type certSelectTest struct {
		name          string
		testType      testType
		minVersion    uint16
		maxVersion    uint16
		config        Config
		match         *Credential
		mismatch      *Credential
		flags         []string
		expectedError string
	}
	certSelectTests := []certSelectTest{

		{
			name:       "Server-CipherSuite-ECDHE_ECDSA",
			testType:   serverTest,
			maxVersion: VersionTLS12,
			config: Config{
				CipherSuites: []uint16{
					TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
				},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},
		{
			name:       "Server-CipherSuite-ECDHE_RSA",
			testType:   serverTest,
			maxVersion: VersionTLS12,
			config: Config{
				CipherSuites: []uint16{
					TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
				},
			},
			match:         &rsaCertificate,
			mismatch:      &ecdsaP256Certificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},
		{
			name:       "Server-CipherSuite-RSA",
			testType:   serverTest,
			maxVersion: VersionTLS12,
			config: Config{
				CipherSuites: []uint16{
					TLS_RSA_WITH_AES_128_CBC_SHA,
				},
			},
			match:         &rsaCertificate,
			mismatch:      &ecdsaP256Certificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},

		{
			name:       "Server-CipherSuite-ECDHE_ECDSA-Ed25519",
			testType:   serverTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				CipherSuites: []uint16{
					TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
				},
			},
			match:         &ed25519Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},
		{
			name:       "Server-CipherSuite-ECDHE_RSA-Ed25519",
			testType:   serverTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				CipherSuites: []uint16{
					TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
				},
			},
			match:         &rsaCertificate,
			mismatch:      &ed25519Certificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},

		{
			name:       "Server-CipherSuite-NoECDHE",
			testType:   serverTest,
			maxVersion: VersionTLS12,
			config: Config{
				CurvePreferences: []CurveID{CurveP256},
			},
			flags:         []string{"-curves", strconv.Itoa(int(CurveX25519))},
			match:         &rsaCertificate,
			mismatch:      &ecdsaP256Certificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},

		{
			name:       "Server-CipherSuite-Prefs",
			testType:   serverTest,
			maxVersion: VersionTLS12,
			config: Config{
				CipherSuites: []uint16{
					TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
					TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
				},
			},
			flags:         []string{"-cipher", "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA"},
			match:         &rsaCertificate,
			mismatch:      &ecdsaP256Certificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},

		{
			name:       "Server-Curve",
			testType:   serverTest,
			maxVersion: VersionTLS12,
			config: Config{
				CurvePreferences: []CurveID{CurveP256},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &ecdsaP384Certificate,
			expectedError: ":WRONG_CURVE:",
		},

		{
			name:       "Server-IgnoreCurve",
			testType:   serverTest,
			minVersion: VersionTLS13,
			config: Config{
				CurvePreferences: []CurveID{CurveP256},
			},
			match: &ecdsaP384Certificate,
		},

		{
			name:       "Server-IgnoreCurveEd25519",
			testType:   serverTest,
			minVersion: VersionTLS12,
			config: Config{
				CurvePreferences: []CurveID{CurveP256},
			},
			match: &ed25519Certificate,
		},

		{
			name:       "Server-NoEd25519",
			testType:   serverTest,
			maxVersion: VersionTLS11,
			match:      &rsaCertificate,
			mismatch:   &ed25519Certificate,
		},

		{
			name:       "Server-SignatureAlgorithm",
			testType:   serverTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
				CipherSuites: []uint16{
					TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
					TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
				},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":NO_SHARED_CIPHER:",
		},
		{
			name:       "Server-SignatureAlgorithm",
			testType:   serverTest,
			minVersion: VersionTLS13,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
		},

		{
			name:       "Server-SignatureAlgorithmImpactsECDHEOnly",
			testType:   serverTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
				CipherSuites: []uint16{
					TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
					TLS_RSA_WITH_AES_128_CBC_SHA,
				},
			},
			match: &rsaCertificate,
		},

		{
			name:       "Server-SignatureAlgorithmECDSACurve",
			testType:   serverTest,
			minVersion: VersionTLS13,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &ecdsaP384Certificate,
			expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
		},

		{
			name:       "Server-SignatureAlgorithmECDSACurve",
			testType:   serverTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match: &ecdsaP384Certificate,
		},

		{
			name:       "Server-IgnoreSignatureAlgorithm",
			testType:   serverTest,
			maxVersion: VersionTLS11,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match: &rsaCertificate,
		},

		{
			name:       "Server-SignatureAlgorithmKeyPrefs",
			testType:   serverTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPSSWithSHA256},
				CipherSuites:              []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
			},
			match:         rsaChainCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
			mismatch:      rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
			expectedError: ":NO_SHARED_CIPHER:",
		},
		{
			name:       "Server-SignatureAlgorithmKeyPrefs",
			testType:   serverTest,
			minVersion: VersionTLS13,
			config: Config{
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPSSWithSHA256},
			},
			match:         rsaChainCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
			mismatch:      rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
			expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
		},

		{
			name:       "Client-ClientCertificateTypes-RSA",
			testType:   clientTest,
			maxVersion: VersionTLS12,
			config: Config{
				ClientAuth:             RequestClientCert,
				ClientCertificateTypes: []uint8{CertTypeRSASign},
			},
			match:         &rsaCertificate,
			mismatch:      &ecdsaP256Certificate,
			expectedError: ":UNKNOWN_CERTIFICATE_TYPE:",
		},
		{
			name:       "Client-ClientCertificateTypes-ECDSA",
			testType:   clientTest,
			maxVersion: VersionTLS12,
			config: Config{
				ClientAuth:             RequestClientCert,
				ClientCertificateTypes: []uint8{CertTypeECDSASign},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":UNKNOWN_CERTIFICATE_TYPE:",
		},

		{
			name:       "Client-ClientCertificateTypes-RSA-Ed25519",
			testType:   clientTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				ClientAuth:             RequestClientCert,
				ClientCertificateTypes: []uint8{CertTypeRSASign},
			},
			match:         &rsaCertificate,
			mismatch:      &ed25519Certificate,
			expectedError: ":UNKNOWN_CERTIFICATE_TYPE:",
		},
		{
			name:       "Client-ClientCertificateTypes-ECDSA-Ed25519",
			testType:   clientTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				ClientAuth:             RequestClientCert,
				ClientCertificateTypes: []uint8{CertTypeECDSASign},
			},
			match:         &ed25519Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":UNKNOWN_CERTIFICATE_TYPE:",
		},

		{
			name:       "Client-SignatureAlgorithm",
			testType:   clientTest,
			minVersion: VersionTLS12,
			config: Config{
				ClientAuth:                RequestClientCert,
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &rsaCertificate,
			expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
		},

		{
			name:       "Client-SignatureAlgorithmECDSACurve",
			testType:   clientTest,
			minVersion: VersionTLS13,
			config: Config{
				ClientAuth:                RequestClientCert,
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match:         &ecdsaP256Certificate,
			mismatch:      &ecdsaP384Certificate,
			expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
		},

		{
			name:       "Client-SignatureAlgorithmECDSACurve",
			testType:   clientTest,
			minVersion: VersionTLS12,
			maxVersion: VersionTLS12,
			config: Config{
				ClientAuth:                RequestClientCert,
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureECDSAWithP256AndSHA256},
			},
			match: &ecdsaP384Certificate,
		},

		{
			name:       "Client-SignatureAlgorithmKeyPrefs",
			testType:   clientTest,
			minVersion: VersionTLS12,
			config: Config{
				ClientAuth:                RequestClientCert,
				VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPSSWithSHA256},
			},
			match:         rsaChainCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA256),
			mismatch:      rsaCertificate.WithSignatureAlgorithms(signatureRSAPSSWithSHA384),
			expectedError: ":NO_COMMON_SIGNATURE_ALGORITHMS:",
		},

		{
			name:     "Client-DontCheckIssuer",
			testType: clientTest,
			config: Config{
				ClientAuth: RequestClientCert,
				ClientCAs:  makeCertPoolFromRoots(&rsaChainCertificate, &ecdsaP384Certificate),
			},
			match: &ecdsaP256Certificate,
		},
		{
			name:     "Server-DontCheckIssuer",
			testType: serverTest,
			config: Config{
				RootCAs:     makeCertPoolFromRoots(&rsaChainCertificate, &ecdsaP384Certificate),
				SendRootCAs: true,
			},
			match: &ecdsaP256Certificate,
		},

		{
			name:     "Client-CheckIssuer",
			testType: clientTest,
			config: Config{
				ClientAuth: RequestClientCert,
				ClientCAs:  makeCertPoolFromRoots(&rsaChainCertificate, &ecdsaP384Certificate),
			},
			match:         rsaChainCertificate.WithMustMatchIssuer(true),
			mismatch:      ecdsaP256Certificate.WithMustMatchIssuer(true),
			expectedError: ":NO_MATCHING_ISSUER:",
		},
		{
			name:     "Server-CheckIssuer",
			testType: serverTest,
			config: Config{
				RootCAs:     makeCertPoolFromRoots(&rsaChainCertificate, &ecdsaP384Certificate),
				SendRootCAs: true,
			},
			match:         rsaChainCertificate.WithMustMatchIssuer(true),
			mismatch:      ecdsaP256Certificate.WithMustMatchIssuer(true),
			expectedError: ":NO_MATCHING_ISSUER:",
		},

		{
			name:       "Server-CheckIssuer-TrustAnchorIDs",
			testType:   serverTest,
			minVersion: VersionTLS13,
			config: Config{
				RequestTrustAnchors: [][]byte{{1, 1, 1}},
			},
			match:         rsaChainCertificate.WithTrustAnchorID([]byte{1, 1, 1}),
			mismatch:      ecdsaP256Certificate.WithTrustAnchorID([]byte{2, 2, 2}),
			expectedError: ":NO_MATCHING_ISSUER:",
		},

		{
			name:     "Client-CheckIssuerFallback",
			testType: clientTest,
			config: Config{
				ClientAuth: RequestClientCert,
				ClientCAs:  makeCertPoolFromRoots(&ecdsaP384Certificate),
			},
			match:         &rsaChainCertificate,
			mismatch:      ecdsaP256Certificate.WithMustMatchIssuer(true),
			expectedError: ":NO_MATCHING_ISSUER:",
		},
		{
			name:     "Server-CheckIssuerFallback",
			testType: serverTest,
			config: Config{
				RootCAs:     makeCertPoolFromRoots(&ecdsaP384Certificate),
				SendRootCAs: true,
			},
			match:         &rsaChainCertificate,
			mismatch:      ecdsaP256Certificate.WithMustMatchIssuer(true),
			expectedError: ":NO_MATCHING_ISSUER:",
		},
		{
			name:       "Server-CheckIssuerFallback-TrustAnchorIDs",
			testType:   serverTest,
			minVersion: VersionTLS13,
			config: Config{
				RequestTrustAnchors: [][]byte{{1, 1, 1}},
			},
			match:         &rsaChainCertificate,
			mismatch:      ecdsaP256Certificate.WithTrustAnchorID([]byte{2, 2, 2}),
			expectedError: ":NO_MATCHING_ISSUER:",
		},
	}

	for _, protocol := range []protocol{tls, dtls} {
		for _, vers := range allVersions(protocol) {
			suffix := fmt.Sprintf("%s-%s", protocol, vers)

			testCases = append(testCases, testCase{
				name:     fmt.Sprintf("CertificateSelection-Client-PreferenceOrder-%s", suffix),
				testType: clientTest,
				protocol: protocol,
				config: Config{
					MinVersion: vers.version,
					MaxVersion: vers.version,
					ClientAuth: RequestClientCert,
				},
				shimCredentials: []*Credential{&ecdsaP256Certificate, &ecdsaP384Certificate},
				shimCertificate: &rsaCertificate,
				flags:           []string{"-expect-selected-credential", "0"},
				expectations:    connectionExpectations{peerCertificate: &ecdsaP256Certificate},
			})
			testCases = append(testCases, testCase{
				name:     fmt.Sprintf("CertificateSelection-Server-PreferenceOrder-%s", suffix),
				testType: serverTest,
				protocol: protocol,
				config: Config{
					MinVersion: vers.version,
					MaxVersion: vers.version,
				},
				shimCredentials: []*Credential{&ecdsaP256Certificate, &ecdsaP384Certificate},
				shimCertificate: &rsaCertificate,
				flags:           []string{"-expect-selected-credential", "0"},
				expectations:    connectionExpectations{peerCertificate: &ecdsaP256Certificate},
			})

			testCases = append(testCases, testCase{
				name:     fmt.Sprintf("CertificateSelection-Server-OCSP-SCT-%s", suffix),
				testType: serverTest,
				protocol: protocol,
				config: Config{
					MinVersion: vers.version,
					MaxVersion: vers.version,

					CipherSuites: []uint16{
						TLS_AES_128_GCM_SHA256,
						TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
						TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
					},
					VerifySignatureAlgorithms: []signatureAlgorithm{signatureRSAPSSWithSHA256},
				},
				shimCredentials: []*Credential{
					ecdsaP256Certificate.WithOCSP(testOCSPResponse2).WithSCTList(testSCTList2),
					rsaChainCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
				},
				shimCertificate: ecdsaP384Certificate.WithOCSP(testOCSPResponse2).WithSCTList(testSCTList2),
				flags:           []string{"-expect-selected-credential", "1"},
				expectations: connectionExpectations{
					peerCertificate: rsaChainCertificate.WithOCSP(testOCSPResponse).WithSCTList(testSCTList),
				},
			})

			testCases = append(testCases, testCase{
				name:     fmt.Sprintf("CertificateSelection-Client-Async-%s", suffix),
				testType: clientTest,
				protocol: protocol,
				config: Config{
					MinVersion: vers.version,
					MaxVersion: vers.version,
					ClientAuth: RequestClientCert,
				},
				shimCredentials: []*Credential{&ecdsaP256Certificate},
				shimCertificate: &rsaCertificate,
				flags:           []string{"-async", "-expect-selected-credential", "0"},
				expectations:    connectionExpectations{peerCertificate: &ecdsaP256Certificate},
			})
			testCases = append(testCases, testCase{
				name:     fmt.Sprintf("CertificateSelection-Server-Async-%s", suffix),
				testType: serverTest,
				protocol: protocol,
				config: Config{
					MinVersion: vers.version,
					MaxVersion: vers.version,
				},
				shimCredentials: []*Credential{&ecdsaP256Certificate},
				shimCertificate: &rsaCertificate,
				flags:           []string{"-async", "-expect-selected-credential", "0"},
				expectations:    connectionExpectations{peerCertificate: &ecdsaP256Certificate},
			})

			for _, test := range certSelectTests {
				if test.minVersion != 0 && vers.version < test.minVersion {
					continue
				}
				if test.maxVersion != 0 && vers.version > test.maxVersion {
					continue
				}

				config := test.config
				config.MinVersion = vers.version
				config.MaxVersion = vers.version

				if test.mismatch == nil {
					testCases = append(testCases, testCase{
						name:            fmt.Sprintf("CertificateSelection-%s-%s", test.name, suffix),
						protocol:        protocol,
						testType:        test.testType,
						config:          config,
						shimCredentials: []*Credential{test.match},
						flags:           append([]string{"-expect-selected-credential", "0"}, test.flags...),
						expectations:    connectionExpectations{peerCertificate: test.match},
					})
					continue
				}

				testCases = append(testCases, testCase{
					name:            fmt.Sprintf("CertificateSelection-%s-MatchFirst-%s", test.name, suffix),
					protocol:        protocol,
					testType:        test.testType,
					config:          config,
					shimCredentials: []*Credential{test.match, test.mismatch},
					flags:           append([]string{"-expect-selected-credential", "0"}, test.flags...),
					expectations:    connectionExpectations{peerCertificate: test.match},
				})
				testCases = append(testCases, testCase{
					name:            fmt.Sprintf("CertificateSelection-%s-MatchSecond-%s", test.name, suffix),
					protocol:        protocol,
					testType:        test.testType,
					config:          config,
					shimCredentials: []*Credential{test.mismatch, test.match},
					flags:           append([]string{"-expect-selected-credential", "1"}, test.flags...),
					expectations:    connectionExpectations{peerCertificate: test.match},
				})
				if canBeShimCertificate(test.match) {
					testCases = append(testCases, testCase{
						name:            fmt.Sprintf("CertificateSelection-%s-MatchDefault-%s", test.name, suffix),
						protocol:        protocol,
						testType:        test.testType,
						config:          config,
						shimCredentials: []*Credential{test.mismatch},
						shimCertificate: test.match,
						flags:           append([]string{"-expect-selected-credential", "-1"}, test.flags...),
						expectations:    connectionExpectations{peerCertificate: test.match},
					})
				}
				testCases = append(testCases, testCase{
					name:               fmt.Sprintf("CertificateSelection-%s-MatchNone-%s", test.name, suffix),
					protocol:           protocol,
					testType:           test.testType,
					config:             config,
					shimCredentials:    []*Credential{test.mismatch, test.mismatch, test.mismatch},
					flags:              test.flags,
					shouldFail:         true,
					expectedLocalError: "remote error: handshake failure",
					expectedError:      test.expectedError,
				})
			}
		}
	}
}
