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
	"math/big"
	"strconv"
	"strings"
	"time"

	"boringssl.googlesource.com/boringssl.git/ssl/test/runner/hpke"
)

type echCipher struct {
	name   string
	cipher HPKECipherSuite
}

var echCiphers = []echCipher{
	{
		name:   "HKDF-SHA256-AES-128-GCM",
		cipher: HPKECipherSuite{KDF: hpke.HKDFSHA256, AEAD: hpke.AES128GCM},
	},
	{
		name:   "HKDF-SHA256-AES-256-GCM",
		cipher: HPKECipherSuite{KDF: hpke.HKDFSHA256, AEAD: hpke.AES256GCM},
	},
	{
		name:   "HKDF-SHA256-ChaCha20-Poly1305",
		cipher: HPKECipherSuite{KDF: hpke.HKDFSHA256, AEAD: hpke.ChaCha20Poly1305},
	},
}

func generateServerECHConfig(template *ECHConfig) ServerECHConfig {
	publicKey, secretKey, err := hpke.GenerateKeyPairX25519()
	if err != nil {
		panic(err)
	}
	templateCopy := *template
	if templateCopy.KEM == 0 {
		templateCopy.KEM = hpke.X25519WithHKDFSHA256
	}
	if len(templateCopy.PublicKey) == 0 {
		templateCopy.PublicKey = publicKey
	}
	if len(templateCopy.CipherSuites) == 0 {
		templateCopy.CipherSuites = make([]HPKECipherSuite, len(echCiphers))
		for i, cipher := range echCiphers {
			templateCopy.CipherSuites[i] = cipher.cipher
		}
	}
	if len(templateCopy.PublicName) == 0 {
		templateCopy.PublicName = "public.example"
	}
	if templateCopy.MaxNameLen == 0 {
		templateCopy.MaxNameLen = 64
	}
	return ServerECHConfig{ECHConfig: CreateECHConfig(&templateCopy), Key: secretKey}
}

func addEncryptedClientHelloTests() {

	echConfig := generateServerECHConfig(&ECHConfig{ConfigID: 42})
	echConfig1 := generateServerECHConfig(&ECHConfig{ConfigID: 43})
	echConfig2 := generateServerECHConfig(&ECHConfig{ConfigID: 44})
	echConfig3 := generateServerECHConfig(&ECHConfig{ConfigID: 45})
	echConfigRepeatID := generateServerECHConfig(&ECHConfig{ConfigID: 42})

	echSecretCertificate := generateSingleCertChain(&x509.Certificate{
		SerialNumber: big.NewInt(57005),
		Subject: pkix.Name{
			CommonName: "test cert",
		},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(time.Hour),
		DNSNames:              []string{"secret.example"},
		IsCA:                  true,
		BasicConstraintsValid: true,
	}, &rsa2048Key)
	echPublicCertificate := generateSingleCertChain(&x509.Certificate{
		SerialNumber: big.NewInt(57005),
		Subject: pkix.Name{
			CommonName: "test cert",
		},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(time.Hour),
		DNSNames:              []string{"public.example"},
		IsCA:                  true,
		BasicConstraintsValid: true,
	}, &rsa2048Key)
	echLongNameCertificate := generateSingleCertChain(&x509.Certificate{
		SerialNumber: big.NewInt(57005),
		Subject: pkix.Name{
			CommonName: "test cert",
		},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(time.Hour),
		DNSNames:              []string{"test0123456789.example"},
		IsCA:                  true,
		BasicConstraintsValid: true,
	}, &ecdsaP256Key)

	for _, protocol := range []protocol{tls, quic, dtls} {
		prefix := protocol.String() + "-"

		for _, hrr := range []bool{false, true} {
			var suffix string
			var defaultCurves []CurveID
			if hrr {
				suffix = "-HelloRetryRequest"

				defaultCurves = []CurveID{}
			}

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server" + suffix,
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
					DefaultCurves:   defaultCurves,
				},
				resumeSession: true,
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-MinimalClientHelloOuter" + suffix,
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
					DefaultCurves:   defaultCurves,
					Bugs: ProtocolBugs{
						MinimalClientHelloOuter: true,
					},
				},
				resumeSession: true,
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-Decline" + suffix,
				config: Config{
					ServerName:    "secret.example",
					DefaultCurves: defaultCurves,

					ClientECHConfig: echConfig.ECHConfig,
					Bugs: ProtocolBugs{
						OfferSessionInClientHelloOuter: true,
						ExpectECHRetryConfigs:          CreateECHConfigList(echConfig2.ECHConfig.Raw, echConfig3.ECHConfig.Raw),
					},
				},
				resumeSession: true,
				flags: []string{

					"-ech-server-config", base64FlagValue(echConfig1.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig1.Key),
					"-ech-is-retry-config", "0",
					"-ech-server-config", base64FlagValue(echConfig2.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig2.Key),
					"-ech-is-retry-config", "1",
					"-ech-server-config", base64FlagValue(echConfig3.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig3.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "public.example",
				},
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-TLS12InInner" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					Bugs: ProtocolBugs{
						AllowTLS12InClientHelloInner: true,
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":INVALID_CLIENT_HELLO_INNER:",
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-MissingECHInner" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					Bugs: ProtocolBugs{
						OmitECHInner:       !hrr,
						OmitSecondECHInner: hrr,
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":INVALID_CLIENT_HELLO_INNER:",
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-OuterExtensions" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					ECHOuterExtensions: []uint16{
						extensionKeyShare,
						extensionSupportedCurves,

						extensionCustom,
					},
					Bugs: ProtocolBugs{
						CustomExtension:                    "test",
						OnlyCompressSecondClientHelloInner: hrr,
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-OuterExtensions-Interleaved" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					ECHOuterExtensions: []uint16{
						extensionKeyShare,
						extensionSupportedCurves,
						extensionCustom,
					},
					Bugs: ProtocolBugs{
						CustomExtension:                    "test",
						OnlyCompressSecondClientHelloInner: hrr,
						ECHOuterExtensionOrder: []uint16{
							extensionServerName,
							extensionKeyShare,
							extensionSupportedVersions,
							extensionPSKKeyExchangeModes,
							extensionSupportedCurves,
							extensionSignatureAlgorithms,
							extensionCustom,
						},
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-OuterExtensions-WrongOrder" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					ECHOuterExtensions: []uint16{
						extensionKeyShare,
						extensionSupportedCurves,
					},
					Bugs: ProtocolBugs{
						CustomExtension:                    "test",
						OnlyCompressSecondClientHelloInner: hrr,
						ECHOuterExtensionOrder: []uint16{
							extensionSupportedCurves,
							extensionKeyShare,
						},
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":INVALID_OUTER_EXTENSION:",
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-OuterExtensions-Duplicate" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					ECHOuterExtensions: []uint16{
						extensionSupportedCurves,
						extensionSupportedCurves,
					},
					Bugs: ProtocolBugs{
						OnlyCompressSecondClientHelloInner: hrr,

						ECHOuterExtensionOrder: []uint16{
							extensionSupportedCurves,
						},
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":INVALID_OUTER_EXTENSION:",
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-OuterExtensions-Missing" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					ECHOuterExtensions: []uint16{
						extensionCustom,
					},
					Bugs: ProtocolBugs{
						OnlyCompressSecondClientHelloInner: hrr,
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
					"-expect-ech-accept",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":INVALID_OUTER_EXTENSION:",
			})

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-OuterExtensions-SelfReference" + suffix,
				config: Config{
					ServerName:      "secret.example",
					DefaultCurves:   defaultCurves,
					ClientECHConfig: echConfig.ECHConfig,
					ECHOuterExtensions: []uint16{
						extensionEncryptedClientHello,
					},
					Bugs: ProtocolBugs{
						OnlyCompressSecondClientHelloInner: hrr,
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: illegal parameter",
				expectedError:      ":INVALID_OUTER_EXTENSION:",
			})

			clientAndServerHello := "read hs 1\nread clienthelloinner\nwrite hs 2\n"
			expectMsgCallback := clientAndServerHello
			if protocol == tls {
				expectMsgCallback += "write ccs\n"
			}
			if hrr {
				expectMsgCallback += clientAndServerHello
			}

			expectMsgCallback += `write hs 8
write hs 11
write hs 15
write hs 20
read hs 20
write ack
write hs 4
write hs 4
read ack
read ack
`
			if protocol != dtls {
				expectMsgCallback = strings.ReplaceAll(expectMsgCallback, "write ack\n", "")
				expectMsgCallback = strings.ReplaceAll(expectMsgCallback, "read ack\n", "")
			}
			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-MessageCallback" + suffix,
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
					DefaultCurves:   defaultCurves,
					Bugs: ProtocolBugs{
						NoCloseNotify: true,
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-ech-accept",
					"-expect-msg-callback", expectMsgCallback,
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})
		}

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-AsyncEarlyCallback",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,
			},
			flags: []string{
				"-async",
				"-use-early-callback",
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-server-name", "secret.example",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-FailCallbackNeedRewind",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,
			},
			flags: []string{
				"-async",
				"-fail-early-callback-ech-rewind",
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-server-name", "public.example",
			},
			expectations: connectionExpectations{
				echAccepted: false,
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-RewindWithNoPublicName",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,
				Bugs: ProtocolBugs{
					OmitPublicName: true,
				},
			},
			flags: []string{
				"-async",
				"-fail-early-callback-ech-rewind",
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-no-server-name",
			},
			expectations: connectionExpectations{
				echAccepted: false,
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-SecondECHConfig",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig1.ECHConfig,
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-ech-server-config", base64FlagValue(echConfig1.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig1.Key),
				"-ech-is-retry-config", "1",
				"-expect-server-name", "secret.example",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-RepeatedConfigID",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfigRepeatID.ECHConfig,
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-ech-server-config", base64FlagValue(echConfigRepeatID.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfigRepeatID.Key),
				"-ech-is-retry-config", "1",
				"-expect-server-name", "secret.example",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
		})

		for i, cipher := range echCiphers {
			otherCipher := echCiphers[(i+1)%len(echCiphers)]

			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-Cipher-" + cipher.name,
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
					ECHCipherSuites: []HPKECipherSuite{cipher.cipher},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "secret.example",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})

			cipherConfig := generateServerECHConfig(&ECHConfig{
				ConfigID: 42,
				CipherSuites: []HPKECipherSuite{
					{KDF: 0x1111, AEAD: 0x2222},
					{KDF: cipher.cipher.KDF, AEAD: 0x2222},
					{KDF: 0x1111, AEAD: cipher.cipher.AEAD},
					cipher.cipher,
				},
			})
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Cipher-" + cipher.name,
				config: Config{
					ServerECHConfigs: []ServerECHConfig{cipherConfig},
					Credential:       &echSecretCertificate,
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(cipherConfig.ECHConfig.Raw)),
					"-host-name", "secret.example",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})

			otherCipherConfig := generateServerECHConfig(&ECHConfig{
				ConfigID:     42,
				CipherSuites: []HPKECipherSuite{otherCipher.cipher},
			})
			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-DisabledCipher-" + cipher.name,
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
					ECHCipherSuites: []HPKECipherSuite{cipher.cipher},
					Bugs: ProtocolBugs{
						ExpectECHRetryConfigs: CreateECHConfigList(otherCipherConfig.ECHConfig.Raw),
					},
				},
				flags: []string{
					"-ech-server-config", base64FlagValue(otherCipherConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(otherCipherConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-server-name", "public.example",
				},
			})
		}

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-ShortEnc",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,
				Bugs: ProtocolBugs{
					ExpectECHRetryConfigs: CreateECHConfigList(echConfig.ECHConfig.Raw),
					TruncateClientECHEnc:  true,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-server-name", "public.example",
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-CorruptEncryptedClientHello",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,
				Bugs: ProtocolBugs{
					ExpectECHRetryConfigs:       CreateECHConfigList(echConfig.ECHConfig.Raw),
					CorruptEncryptedClientHello: true,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-CorruptSecondEncryptedClientHello",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,

				DefaultCurves: []CurveID{},
				Bugs: ProtocolBugs{
					CorruptSecondEncryptedClientHello: true,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
			},
			shouldFail:         true,
			expectedError:      ":DECRYPTION_FAILED:",
			expectedLocalError: "remote error: error decrypting message",
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-OmitSecondEncryptedClientHello",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,

				DefaultCurves: []CurveID{},
				Bugs: ProtocolBugs{
					OmitSecondEncryptedClientHello: true,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
			},
			shouldFail:         true,
			expectedError:      ":MISSING_EXTENSION:",
			expectedLocalError: "remote error: missing extension",
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-DifferentConfigIDSecondClientHello",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,

				DefaultCurves: []CurveID{},
				Bugs: ProtocolBugs{
					CorruptSecondEncryptedClientHelloConfigID: true,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
			},
			shouldFail:         true,
			expectedError:      ":DECODE_ERROR:",
			expectedLocalError: "remote error: illegal parameter",
		})

		if protocol != dtls {
			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-EarlyData",
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
				},
				resumeSession: true,
				earlyData:     true,
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})
			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-EarlyDataRejected",
				config: Config{
					ServerName:      "secret.example",
					ClientECHConfig: echConfig.ECHConfig,
					Bugs: ProtocolBugs{

						SendTicketAge: 1 * time.Hour,
					},
				},
				resumeSession:           true,
				earlyData:               true,
				expectEarlyDataRejected: true,
				flags: []string{
					"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
					"-ech-server-key", base64FlagValue(echConfig.Key),
					"-ech-is-retry-config", "1",
					"-expect-ech-accept",
				},
				expectations: connectionExpectations{
					echAccepted: true,
				},
			})
		}

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-Disabled",
			config: Config{
				ServerName:      "secret.example",
				ClientECHConfig: echConfig.ECHConfig,
			},
			flags: []string{
				"-expect-server-name", "public.example",
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-ClientAuth",
			config: Config{
				Credential:      &rsaCertificate,
				ClientECHConfig: echConfig.ECHConfig,
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-ech-accept",
				"-require-any-client-certificate",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-Decline-ClientAuth",
			config: Config{
				Credential:      &rsaCertificate,
				ClientECHConfig: echConfig.ECHConfig,
				Bugs: ProtocolBugs{
					ExpectECHRetryConfigs: CreateECHConfigList(echConfig1.ECHConfig.Raw),
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig1.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig1.Key),
				"-ech-is-retry-config", "1",
				"-require-any-client-certificate",
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-Padding",
			config: Config{
				ClientECHConfig: echConfig.ECHConfig,
				Bugs: ProtocolBugs{
					ClientECHPadding: 10,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-BadPadding",
			config: Config{
				ClientECHConfig: echConfig.ECHConfig,
				Bugs: ProtocolBugs{
					ClientECHPadding:    10,
					BadClientECHPadding: true,
				},
			},
			flags: []string{
				"-ech-server-config", base64FlagValue(echConfig.ECHConfig.Raw),
				"-ech-server-key", base64FlagValue(echConfig.Key),
				"-ech-is-retry-config", "1",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
			shouldFail:         true,
			expectedError:      ":DECODE_ERROR",
			expectedLocalError: "remote error: illegal parameter",
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-GREASE-Client-TLS13",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					ExpectClientECH: true,
				},
			},
			flags: []string{"-enable-ech-grease"},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-GREASE-Client-TLS13-HelloRetryRequest",
			config: Config{
				MaxVersion: VersionTLS13,
				MinVersion: VersionTLS13,

				CurvePreferences: []CurveID{CurveP384},
				Bugs: ProtocolBugs{
					ExpectMissingKeyShare: true,
					ExpectClientECH:       true,
				},
			},
			flags: []string{"-enable-ech-grease", "-expect-hrr"},
		})

		unsupportedVersion := []byte{

			0xba, 0xdd,

			0x00, 0x05,

			0x05, 0x04, 0x03, 0x02, 0x01,
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-GREASE-Client-TLS13-Retry-Configs",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					ExpectClientECH: true,

					SendECHRetryConfigs: CreateECHConfigList(echConfig.ECHConfig.Raw, unsupportedVersion),
				},
			},
			flags: []string{"-enable-ech-grease"},
		})

		if protocol != quic {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-GREASE-Client-TLS12-RejectRetryConfigs",
				config: Config{
					MinVersion:       VersionTLS12,
					MaxVersion:       VersionTLS12,
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectClientECH:           true,
						AlwaysSendECHRetryConfigs: true,
					},
				},
				flags:              []string{"-enable-ech-grease"},
				shouldFail:         true,
				expectedLocalError: "remote error: unsupported extension",
				expectedError:      ":UNEXPECTED_EXTENSION:",
			})
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-TLS12-RejectRetryConfigs",
				config: Config{
					MinVersion:       VersionTLS12,
					MaxVersion:       VersionTLS12,
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectClientECH:           true,
						AlwaysSendECHRetryConfigs: true,
					},
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig1.ECHConfig.Raw)),
				},
				shouldFail:         true,
				expectedLocalError: "remote error: unsupported extension",
				expectedError:      ":UNEXPECTED_EXTENSION:",
			})
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Accept-RejectRetryConfigs",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectClientECH:           true,
					AlwaysSendECHRetryConfigs: true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
			},
			shouldFail:         true,
			expectedLocalError: "remote error: unsupported extension",
			expectedError:      ":UNEXPECTED_EXTENSION:",
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-UnsolictedHRRExtension",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				CurvePreferences: []CurveID{CurveP384},
				Bugs: ProtocolBugs{
					AlwaysSendECHHelloRetryRequest: true,
					ExpectMissingKeyShare:          true,
				},
			},
			shouldFail:         true,
			expectedLocalError: "remote error: unsupported extension",
			expectedError:      ":UNEXPECTED_EXTENSION:",
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-GREASE-IgnoreHRRExtension",
			config: Config{
				CurvePreferences: []CurveID{CurveP384},
				Bugs: ProtocolBugs{
					AlwaysSendECHHelloRetryRequest: true,
					ExpectMissingKeyShare:          true,
				},
			},
			flags: []string{"-enable-ech-grease"},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-RandomHRRExtension",
			config: Config{
				CurvePreferences: []CurveID{CurveP384},
				Bugs: ProtocolBugs{
					AlwaysSendECHHelloRetryRequest: true,
					ExpectMissingKeyShare:          true,
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
			},
			shouldFail:         true,
			expectedLocalError: "remote error: ECH required",
			expectedError:      ":ECH_REJECTED:",
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-GREASE-Client-TLS13-Invalid-Retry-Configs",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					ExpectClientECH:     true,
					SendECHRetryConfigs: []byte{0xba, 0xdd, 0xec, 0xcc},
				},
			},
			flags:              []string{"-enable-ech-grease"},
			shouldFail:         true,
			expectedLocalError: "remote error: error decoding message",
			expectedError:      ":ERROR_PARSING_EXTENSION:",
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-ECHInner",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					AlwaysSendECHInner: true,
				},
			},
			resumeSession: true,
		})
		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-ECHInner-HelloRetryRequest",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,

				DefaultCurves: []CurveID{},
				Bugs: ProtocolBugs{
					AlwaysSendECHInner: true,
				},
			},
			resumeSession: true,
		})

		testCases = append(testCases, testCase{
			testType: serverTest,
			protocol: protocol,
			name:     prefix + "ECH-Server-ECHInner-NotEmpty",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Bugs: ProtocolBugs{
					AlwaysSendECHInner:  true,
					SendInvalidECHInner: []byte{42, 42, 42},
				},
			},
			shouldFail:         true,
			expectedLocalError: "remote error: error decoding message",
			expectedError:      ":ERROR_PARSING_EXTENSION:",
		})

		if protocol != quic {
			testCases = append(testCases, testCase{
				testType: serverTest,
				protocol: protocol,
				name:     prefix + "ECH-Server-ECHInner-Absent-TLS12",
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS13,
					Bugs: ProtocolBugs{

						OmitSupportedVersions: true,
						AlwaysSendECHInner:    true,
					},
				},

				shouldFail:         true,
				expectedLocalError: "tls: downgrade from TLS 1.3 detected",
			})
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client",
			config: Config{
				MinVersion:       VersionTLS13,
				MaxVersion:       VersionTLS13,
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectServerName:      "secret.example",
					ExpectOuterServerName: "public.example",
				},
				Credential: &echSecretCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-host-name", "secret.example",
				"-expect-ech-accept",
			},
			resumeSession: true,
			expectations:  connectionExpectations{echAccepted: true},
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-HelloRetryRequest",
			config: Config{
				MinVersion:       VersionTLS13,
				MaxVersion:       VersionTLS13,
				CurvePreferences: []CurveID{CurveP384},
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectServerName:      "secret.example",
					ExpectOuterServerName: "public.example",
					ExpectMissingKeyShare: true,
				},
				Credential: &echSecretCertificate,
			},
			resumeSession: true,
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-host-name", "secret.example",
				"-expect-ech-accept",
				"-expect-hrr",
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		if protocol != dtls {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-EarlyData",
				config: Config{
					MinVersion:       VersionTLS13,
					MaxVersion:       VersionTLS13,
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectServerName: "secret.example",
					},
					Credential: &echSecretCertificate,
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-host-name", "secret.example",
					"-expect-ech-accept",
				},
				resumeSession: true,
				earlyData:     true,
				expectations:  connectionExpectations{echAccepted: true},
			})
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-EarlyDataRejected",
				config: Config{
					MinVersion:       VersionTLS13,
					MaxVersion:       VersionTLS13,
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectServerName:      "secret.example",
						AlwaysRejectEarlyData: true,
					},
					Credential: &echSecretCertificate,
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-host-name", "secret.example",
					"-expect-ech-accept",
				},
				resumeSession:           true,
				earlyData:               true,
				expectEarlyDataRejected: true,
				expectations:            connectionExpectations{echAccepted: true},
			})
		}

		if protocol != quic {

			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-TLS12SessionID",
				config: Config{
					MaxVersion:             VersionTLS12,
					SessionTicketsDisabled: true,
				},
				resumeConfig: &Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectNoTLS12Session: true,
					},
				},
				flags: []string{
					"-on-resume-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-on-resume-expect-ech-accept",
				},
				resumeSession:        true,
				expectResumeRejected: true,
				resumeExpectations:   &connectionExpectations{echAccepted: true},
			})
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-TLS12SessionTicket",
				config: Config{
					MaxVersion: VersionTLS12,
				},
				resumeConfig: &Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectNoTLS12Session: true,
					},
				},
				flags: []string{
					"-on-resume-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-on-resume-expect-ech-accept",
				},
				resumeSession:        true,
				expectResumeRejected: true,
				resumeExpectations:   &connectionExpectations{echAccepted: true},
			})
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-NoNPN",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",

				"-select-next-proto", "foo",
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		unsupportedKEM := generateServerECHConfig(&ECHConfig{
			KEM:       0x6666,
			PublicKey: []byte{1, 2, 3, 4},
		}).ECHConfig
		unsupportedCipherSuites := generateServerECHConfig(&ECHConfig{
			CipherSuites: []HPKECipherSuite{{0x1111, 0x2222}},
		}).ECHConfig
		unsupportedMandatoryExtension := generateServerECHConfig(&ECHConfig{
			UnsupportedMandatoryExtension: true,
		}).ECHConfig
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-SelectECHConfig",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(
					unsupportedVersion,
					unsupportedKEM.Raw,
					unsupportedCipherSuites.Raw,
					unsupportedMandatoryExtension.Raw,
					echConfig.ECHConfig.Raw,

					echConfig1.ECHConfig.Raw,
				)),
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{
				echAccepted: true,
			},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-NoSupportedConfigs",
			config: Config{
				Bugs: ProtocolBugs{
					ExpectNoClientECH: true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(
					unsupportedVersion,
					unsupportedKEM.Raw,
					unsupportedCipherSuites.Raw,
					unsupportedMandatoryExtension.Raw,
				)),
			},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-NoSupportedConfigs-GREASE",
			config: Config{
				Bugs: ProtocolBugs{
					ExpectClientECH: true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(
					unsupportedVersion,
					unsupportedKEM.Raw,
					unsupportedCipherSuites.Raw,
					unsupportedMandatoryExtension.Raw,
				)),
				"-enable-ech-grease",
			},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-GREASE",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",
			},
			resumeSession: true,
			expectations:  connectionExpectations{echAccepted: true},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-GREASEExtensions",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectGREASE: true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",
				"-enable-grease",
			},
			resumeSession: true,
			expectations:  connectionExpectations{echAccepted: true},
		})

		unsupportedExtension := generateServerECHConfig(&ECHConfig{UnsupportedExtension: true})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-UnsupportedExtension",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{unsupportedExtension},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(unsupportedExtension.ECHConfig.Raw)),
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-InvalidECHConfigList",
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw[1:])),
			},
			shouldFail:    true,
			expectedError: ":INVALID_ECH_CONFIG_LIST:",
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-UnsolicitedInnerServerNameAck",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{

					ExpectOuterServerName: "public.example",

					SendServerNameAck: true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),

				"-expect-ech-accept",
			},
			shouldFail:         true,
			expectedError:      ":UNEXPECTED_EXTENSION:",
			expectedLocalError: "remote error: unsupported extension",
			expectations:       connectionExpectations{echAccepted: true},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-ExpectECHOuterExtensions",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				NextProtos:       []string{"proto"},
				Bugs: ProtocolBugs{
					ExpectECHOuterExtensions: []uint16{
						extensionALPN,
						extensionKeyShare,
						extensionPSKKeyExchangeModes,
						extensionSignatureAlgorithms,
						extensionSupportedCurves,
					},
				},
				Credential: &echSecretCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",
				"-advertise-alpn", "\x05proto",
				"-expect-alpn", "proto",
				"-host-name", "secret.example",
			},
			expectations: connectionExpectations{
				echAccepted: true,
				nextProto:   "proto",
			},
			skipQUICALPNConfig: true,
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-NeverCompressServerName",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				NextProtos:       []string{"proto"},
				Bugs: ProtocolBugs{
					ExpectECHUncompressedExtensions: []uint16{extensionServerName},
					ExpectServerName:                "public.example",
					ExpectOuterServerName:           "public.example",
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",
				"-host-name", "public.example",
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		tls13Vers := VersionTLS13
		if protocol == dtls {
			tls13Vers = VersionDTLS13
		}
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-CompressSupportedVersions",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectECHOuterExtensions: []uint16{
						extensionSupportedVersions,
					},
				},
				Credential: &echSecretCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-host-name", "secret.example",
				"-expect-ech-accept",
				"-min-version", strconv.Itoa(int(tls13Vers)),
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		maxNameLen10 := generateServerECHConfig(&ECHConfig{MaxNameLen: 10})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-NameTooLong",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{maxNameLen10},
				Bugs: ProtocolBugs{
					ExpectServerName: "test0123456789.example",
				},
				Credential: &echLongNameCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(maxNameLen10.ECHConfig.Raw)),
				"-host-name", "test0123456789.example",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig2, echConfig3},
				Bugs: ProtocolBugs{
					ExpectServerName: "public.example",
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-retry-configs", base64FlagValue(CreateECHConfigList(echConfig2.ECHConfig.Raw, echConfig3.ECHConfig.Raw)),
			},
			shouldFail:         true,
			expectedLocalError: "remote error: ECH required",
			expectedError:      ":ECH_REJECTED:",
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-HelloRetryRequest",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig2, echConfig3},
				CurvePreferences: []CurveID{CurveP384},
				Bugs: ProtocolBugs{
					ExpectServerName:      "public.example",
					ExpectMissingKeyShare: true,
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-retry-configs", base64FlagValue(CreateECHConfigList(echConfig2.ECHConfig.Raw, echConfig3.ECHConfig.Raw)),
				"-expect-hrr",
			},
			shouldFail:         true,
			expectedLocalError: "remote error: ECH required",
			expectedError:      ":ECH_REJECTED:",
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-NoRetryConfigs",
			config: Config{
				Bugs: ProtocolBugs{
					ExpectServerName: "public.example",
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-no-ech-retry-configs",
			},
			shouldFail:         true,
			expectedLocalError: "remote error: ECH required",
			expectedError:      ":ECH_REJECTED:",
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-TLS12",
				config: Config{
					MaxVersion: VersionTLS12,
					Bugs: ProtocolBugs{
						ExpectServerName: "public.example",
					},
					Credential: &echPublicCertificate,
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),

					"-expect-no-ech-retry-configs",
				},
				shouldFail:         true,
				expectedLocalError: "remote error: ECH required",
				expectedError:      ":ECH_REJECTED:",
			})

			testCases = append(testCases, testCase{
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-TLS12-NoFalseStart",
				config: Config{
					MaxVersion:   VersionTLS12,
					CipherSuites: []uint16{TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},
					NextProtos:   []string{"foo"},
					Bugs: ProtocolBugs{

						ExpectFalseStart:          true,
						AlertBeforeFalseStartTest: alertAccessDenied,
					},
					Credential: &echPublicCertificate,
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-false-start",
					"-advertise-alpn", "\x03foo",
					"-expect-alpn", "foo",
				},
				shimWritesFirst: true,
				shouldFail:      true,

				expectedLocalError: "tls: peer did not false start: EOF",

				expectedError: ":TLSV1_ALERT_ACCESS_DENIED:",
			})
		}

		retryConfigs := CreateECHConfigList(
			unsupportedVersion,
			unsupportedKEM.Raw,
			unsupportedCipherSuites.Raw,
			unsupportedMandatoryExtension.Raw,
			echConfig2.ECHConfig.Raw)
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-UnsupportedRetryConfigs",
			config: Config{
				Bugs: ProtocolBugs{
					SendECHRetryConfigs: retryConfigs,
					ExpectServerName:    "public.example",
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-retry-configs", base64FlagValue(retryConfigs),
			},
			shouldFail:         true,
			expectedLocalError: "remote error: ECH required",
			expectedError:      ":ECH_REJECTED:",
		})

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-ResumeInnerSession-TLS13",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectServerName: "secret.example",
				},
				Credential: &echSecretCertificate,
			},
			resumeConfig: &Config{
				MaxVersion:       VersionTLS13,
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectServerName:                    "public.example",
					UseInnerSessionWithClientHelloOuter: true,
				},
				Credential: &echPublicCertificate,
			},
			resumeSession: true,
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-host-name", "secret.example",
				"-on-initial-expect-ech-accept",
			},
			shouldFail:         true,
			expectedError:      ":UNEXPECTED_EXTENSION:",
			expectations:       connectionExpectations{echAccepted: true},
			resumeExpectations: &connectionExpectations{echAccepted: false},
		})
		if protocol == tls {

			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-ResumeInnerSession-TLS12",
				config: Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectServerName: "secret.example",
					},
					Credential: &echSecretCertificate,
				},
				resumeConfig: &Config{
					MinVersion:       VersionTLS12,
					MaxVersion:       VersionTLS12,
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectServerName:                    "public.example",
						UseInnerSessionWithClientHelloOuter: true,

						AcceptAnySession: true,
					},
					Credential: &echPublicCertificate,
				},
				resumeSession: true,
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-host-name", "secret.example",
					"-on-initial-expect-ech-accept",
				},

				shouldFail:         true,
				expectedError:      ":SERVER_ECHOED_INVALID_SESSION_ID:",
				expectedLocalError: "remote error: illegal parameter",
				expectations:       connectionExpectations{echAccepted: true},
				resumeExpectations: &connectionExpectations{echAccepted: false},
			})
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-EarlyDataRejected",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectServerName: "secret.example",
				},
				Credential: &echSecretCertificate,
			},
			resumeConfig: &Config{
				ServerECHConfigs: []ServerECHConfig{echConfig2},
				Bugs: ProtocolBugs{
					ExpectServerName: "public.example",
				},
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-host-name", "secret.example",

				"-expect-ech-accept",

				"-on-retry-expect-ech-retry-configs", base64FlagValue(CreateECHConfigList(echConfig2.ECHConfig.Raw)),
			},
			resumeSession:           true,
			expectResumeRejected:    true,
			earlyData:               true,
			expectEarlyDataRejected: true,
			expectations:            connectionExpectations{echAccepted: true},
			resumeExpectations:      &connectionExpectations{echAccepted: false},
			shouldFail:              true,
			expectedLocalError:      "remote error: ECH required",
			expectedError:           ":ECH_REJECTED:",
		})

		if protocol != quic && protocol != dtls {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-EarlyDataRejected-TLS12",
				config: Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Bugs: ProtocolBugs{
						ExpectServerName: "secret.example",
					},
					Credential: &echSecretCertificate,
				},
				resumeConfig: &Config{
					MaxVersion: VersionTLS12,
					Bugs: ProtocolBugs{
						ExpectServerName: "public.example",
					},
					Credential: &echPublicCertificate,
				},
				flags: []string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-host-name", "secret.example",

					"-expect-ech-accept",
				},
				resumeSession:           true,
				expectResumeRejected:    true,
				earlyData:               true,
				expectEarlyDataRejected: true,
				expectations:            connectionExpectations{echAccepted: true},
				resumeExpectations:      &connectionExpectations{echAccepted: false},

				shouldFail:    true,
				expectedError: ":WRONG_VERSION_ON_EARLY_DATA:",
			})
		}

		invalidPublicName := generateServerECHConfig(&ECHConfig{PublicName: "dns_names_have_no_underscores.example"})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-SkipInvalidPublicName",
			config: Config{
				Bugs: ProtocolBugs{

					ExpectNoClientECH: true,
					ExpectServerName:  "secret.example",
				},
				Credential: &echSecretCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(invalidPublicName.ECHConfig.Raw)),
				"-host-name", "secret.example",
			},
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-SkipInvalidPublicName-2",
			config: Config{

				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectOuterServerName: "public.example",
					ExpectServerName:      "secret.example",
				},
				Credential: &echSecretCertificate,
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(invalidPublicName.ECHConfig.Raw, echConfig.ECHConfig.Raw)),
				"-host-name", "secret.example",
				"-expect-ech-accept",
			},
			expectations: connectionExpectations{echAccepted: true},
		})

		for _, async := range []bool{false, true} {
			var flags []string
			var suffix string
			if async {
				flags = []string{"-async"}
				suffix = "-Async"
			}

			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-ClientCertificate" + suffix,
				config: Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					ClientAuth:       RequireAnyClientCert,
				},
				shimCertificate: &rsaCertificate,
				flags: append([]string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-expect-ech-accept",
				}, flags...),
				expectations: connectionExpectations{echAccepted: true},
			})

			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-NoClientCertificate-TLS13" + suffix,
				config: Config{
					MinVersion: VersionTLS13,
					MaxVersion: VersionTLS13,
					ClientAuth: RequireAnyClientCert,
					Credential: &echPublicCertificate,
				},
				shimCertificate: &rsaCertificate,
				flags: append([]string{
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				}, flags...),
				shouldFail:         true,
				expectedLocalError: "tls: client didn't provide a certificate",
			})
			if protocol != quic {
				testCases = append(testCases, testCase{
					testType: clientTest,
					protocol: protocol,
					name:     prefix + "ECH-Client-Reject-NoClientCertificate-TLS12" + suffix,
					config: Config{
						MinVersion: VersionTLS12,
						MaxVersion: VersionTLS12,
						ClientAuth: RequireAnyClientCert,
						Credential: &echPublicCertificate,
					},
					shimCertificate: &rsaCertificate,
					flags: append([]string{
						"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					}, flags...),
					shouldFail:         true,
					expectedLocalError: "tls: client didn't provide a certificate",
				})
			}
		}

		if protocol != dtls {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-ChannelID",
				config: Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					RequestChannelID: true,
				},
				flags: []string{
					"-send-channel-id", channelIDKeyPath,
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					"-expect-ech-accept",
				},
				resumeSession: true,
				expectations: connectionExpectations{
					channelID:   true,
					echAccepted: true,
				},
			})

			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-NoChannelID-TLS13",
				config: Config{
					MinVersion: VersionTLS13,
					MaxVersion: VersionTLS13,
					Bugs: ProtocolBugs{
						AlwaysNegotiateChannelID: true,
					},
					Credential: &echPublicCertificate,
				},
				flags: []string{
					"-send-channel-id", channelIDKeyPath,
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				},
				shouldFail:         true,
				expectedLocalError: "remote error: unsupported extension",
				expectedError:      ":UNEXPECTED_EXTENSION:",
			})
			if protocol != quic {
				testCases = append(testCases, testCase{
					testType: clientTest,
					protocol: protocol,
					name:     prefix + "ECH-Client-Reject-NoChannelID-TLS12",
					config: Config{
						MinVersion: VersionTLS12,
						MaxVersion: VersionTLS12,
						Bugs: ProtocolBugs{
							AlwaysNegotiateChannelID: true,
						},
						Credential: &echPublicCertificate,
					},
					flags: []string{
						"-send-channel-id", channelIDKeyPath,
						"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
					},
					shouldFail:         true,
					expectedLocalError: "remote error: unsupported extension",
					expectedError:      ":UNEXPECTED_EXTENSION:",
				})
			}
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-NotOffered-NoOverrideName",
			flags: []string{
				"-verify-peer",
				"-use-custom-verify-callback",

				"-reverify-on-resume",
				"-expect-no-ech-name-override",
			},
			resumeSession: true,
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-GREASE-NoOverrideName",
			flags: []string{
				"-verify-peer",
				"-use-custom-verify-callback",
				"-enable-ech-grease",

				"-reverify-on-resume",
				"-expect-no-ech-name-override",
			},
			resumeSession: true,
		})
		if protocol != quic {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Rejected-OverrideName-TLS12",
				config: Config{
					MinVersion: VersionTLS12,
					MaxVersion: VersionTLS12,
				},
				flags: []string{
					"-verify-peer",
					"-use-custom-verify-callback",
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),

					"-expect-ech-name-override", "public.example",
				},
				shouldFail:         true,
				expectedError:      ":ECH_REJECTED:",
				expectedLocalError: "remote error: ECH required",
			})
		}
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Reject-OverrideName-TLS13",
			config: Config{
				MinVersion: VersionTLS13,
				MaxVersion: VersionTLS13,
				Credential: &echPublicCertificate,
			},
			flags: []string{
				"-verify-peer",
				"-use-custom-verify-callback",
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),

				"-expect-ech-name-override", "public.example",
			},
			shouldFail:         true,
			expectedError:      ":ECH_REJECTED:",
			expectedLocalError: "remote error: ECH required",
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-Accept-NoOverrideName",
			config: Config{
				ServerECHConfigs: []ServerECHConfig{echConfig},
			},
			flags: []string{
				"-verify-peer",
				"-use-custom-verify-callback",
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",

				"-reverify-on-resume",
				"-expect-no-ech-name-override",
			},
			resumeSession: true,
			expectations:  connectionExpectations{echAccepted: true},
		})

		if protocol != dtls {
			testCases = append(testCases, testCase{
				testType: clientTest,
				protocol: protocol,
				name:     prefix + "ECH-Client-Reject-EarlyDataRejected-OverrideNameOnRetry",
				config: Config{
					ServerECHConfigs: []ServerECHConfig{echConfig},
					Credential:       &echPublicCertificate,
				},
				resumeConfig: &Config{
					Credential: &echPublicCertificate,
				},
				flags: []string{
					"-verify-peer",
					"-use-custom-verify-callback",
					"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),

					"-expect-ech-accept",

					"-reverify-on-resume",
					"-on-resume-expect-no-ech-name-override",
					"-on-retry-expect-ech-name-override", "public.example",
				},
				resumeSession:           true,
				expectResumeRejected:    true,
				earlyData:               true,
				expectEarlyDataRejected: true,
				expectations:            connectionExpectations{echAccepted: true},
				resumeExpectations:      &connectionExpectations{echAccepted: false},
				shouldFail:              true,
				expectedError:           ":ECH_REJECTED:",
				expectedLocalError:      "remote error: ECH required",
			})
		}

		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-HelloRetryRequest-MissingServerHelloConfirmation",
			config: Config{
				MinVersion:       VersionTLS13,
				MaxVersion:       VersionTLS13,
				CurvePreferences: []CurveID{CurveP384},
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectMissingKeyShare:          true,
					OmitServerHelloECHConfirmation: true,
				},
			},
			resumeSession: true,
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-hrr",
			},
			shouldFail:    true,
			expectedError: ":INCONSISTENT_ECH_NEGOTIATION:",
		})

		clientAndServerHello := "write clienthelloinner\nwrite hs 1\nread hs 2\n"
		clientAndServerHelloInitial := clientAndServerHello
		if protocol == tls {
			clientAndServerHelloInitial += "write ccs\n"
		}

		finishHandshake := `read hs 8
read hs 11
read hs 15
read hs 20
write hs 20
read ack
read hs 4
read hs 4
`
		if protocol != dtls {
			finishHandshake = strings.ReplaceAll(finishHandshake, "read ack\n", "")
		}
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-MessageCallback",
			config: Config{
				MinVersion:       VersionTLS13,
				MaxVersion:       VersionTLS13,
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					NoCloseNotify: true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",
				"-expect-msg-callback", clientAndServerHelloInitial + finishHandshake,
			},
			expectations: connectionExpectations{echAccepted: true},
		})
		testCases = append(testCases, testCase{
			testType: clientTest,
			protocol: protocol,
			name:     prefix + "ECH-Client-MessageCallback-HelloRetryRequest",
			config: Config{
				MinVersion:       VersionTLS13,
				MaxVersion:       VersionTLS13,
				CurvePreferences: []CurveID{CurveP384},
				ServerECHConfigs: []ServerECHConfig{echConfig},
				Bugs: ProtocolBugs{
					ExpectMissingKeyShare: true,
					NoCloseNotify:         true,
				},
			},
			flags: []string{
				"-ech-config-list", base64FlagValue(CreateECHConfigList(echConfig.ECHConfig.Raw)),
				"-expect-ech-accept",
				"-expect-hrr",
				"-expect-msg-callback", clientAndServerHelloInitial + clientAndServerHello + finishHandshake,
			},
			expectations: connectionExpectations{echAccepted: true},
		})
	}
}
