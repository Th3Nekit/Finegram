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

import "golang.org/x/crypto/cryptobyte"

func trustAnchorListFlagValue(ids ...[]byte) string {
	b := cryptobyte.NewBuilder(nil)
	for _, id := range ids {
		addUint8LengthPrefixedBytes(b, id)
	}
	return base64FlagValue(b.BytesOrPanic())
}

func addTrustAnchorTests() {
	id1 := []byte{1}
	id2 := []byte{2, 2}
	id3 := []byte{3, 3, 3}

	testCases = append(testCases, testCase{
		name: "TrustAnchors-Unsolicited-Certificate",
		config: Config{
			MinVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AlwaysMatchTrustAnchorID: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
		expectedError:      ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "TrustAnchors-Unsolicited-EncryptedExtensions",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Bugs: ProtocolBugs{
				AlwaysSendAvailableTrustAnchors: true,
			},
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
		expectedError:      ":UNEXPECTED_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-Match",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Credential:            rsaChainCertificate.WithTrustAnchorID(id1),
			Bugs: ProtocolBugs{
				ExpectPeerRequestedTrustAnchors: [][]byte{id1, id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id1, id3),
			"-expect-peer-match-trust-anchor",
			"-expect-peer-available-trust-anchors", trustAnchorListFlagValue(id1, id2),
		},
	})

	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-Match-Non-Empty-Extension",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Credential:            rsaChainCertificate.WithTrustAnchorID(id1),
			Bugs: ProtocolBugs{
				SendNonEmptyTrustAnchorMatch:    true,
				ExpectPeerRequestedTrustAnchors: [][]byte{id1, id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id1, id3),
		},
		shouldFail:         true,
		expectedLocalError: "remote error: error decoding message",
		expectedError:      ":ERROR_PARSING_EXTENSION:",
	})

	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-Match-On-Incorrect-Certificate",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Credential:            rsaChainCertificate.WithTrustAnchorID(id1),
			Bugs: ProtocolBugs{
				SendTrustAnchorWrongCertificate: true,
				ExpectPeerRequestedTrustAnchors: [][]byte{id1, id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id1, id3),
		},
		shouldFail:         true,
		expectedLocalError: "remote error: unsupported extension",
		expectedError:      ":UNEXPECTED_EXTENSION:",
	})
	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequest-NoMatch",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Bugs: ProtocolBugs{
				ExpectPeerRequestedTrustAnchors: [][]byte{id3},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(id3),
			"-expect-no-peer-match-trust-anchor",
			"-expect-peer-available-trust-anchors", trustAnchorListFlagValue(id1, id2),
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-EmptyID-ClientHello",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{{}},
		},
		shouldFail:    true,
		expectedError: ":DECODE_ERROR:",
	})
	testCases = append(testCases, testCase{
		name: "TrustAnchors-EmptyID-EncryptedExtensions",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{{}},
		},
		flags:         []string{"-requested-trust-anchors", trustAnchorListFlagValue(id1)},
		shouldFail:    true,
		expectedError: ":DECODE_ERROR:",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerSelect-Match",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id2},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2},
				ExpectPeerMatchTrustAnchor:      ptrTo(true),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id1),
			rsaCertificate.WithTrustAnchorID(id2),
		},
		flags: []string{"-expect-selected-credential", "1"},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerSelect-None",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id1},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id2),
			rsaCertificate.WithTrustAnchorID(id3),
		},
		shouldFail:    true,
		expectedError: ":NO_MATCHING_ISSUER:",
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerSelect-Fallback",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{id1},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id2, id3},
				ExpectPeerMatchTrustAnchor:      ptrTo(false),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id2),
			rsaCertificate.WithTrustAnchorID(id3),
			&rsaCertificate,
		},
		flags: []string{"-expect-selected-credential", "2"},
	})

	testCases = append(testCases, testCase{
		name: "TrustAnchors-ClientRequestEmpty",
		config: Config{
			MinVersion:            VersionTLS13,
			AvailableTrustAnchors: [][]byte{id1, id2},
			Bugs: ProtocolBugs{
				ExpectPeerRequestedTrustAnchors: [][]byte{},
			},
		},
		flags: []string{
			"-requested-trust-anchors", trustAnchorListFlagValue(),
			"-expect-peer-available-trust-anchors", trustAnchorListFlagValue(id1, id2),
		},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-ServerReceiveEmptyRequest",
		config: Config{
			MinVersion:          VersionTLS13,
			RequestTrustAnchors: [][]byte{},
			Bugs: ProtocolBugs{
				ExpectPeerAvailableTrustAnchors: [][]byte{id1, id2},
				ExpectPeerMatchTrustAnchor:      ptrTo(false),
			},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id1),
			rsaCertificate.WithTrustAnchorID(id2),
			&rsaCertificate,
		},
		flags: []string{"-expect-selected-credential", "2"},
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "TrustAnchors-TLS12-Server",
		config: Config{
			MaxVersion:          VersionTLS12,
			RequestTrustAnchors: [][]byte{id1},
		},
		shimCredentials: []*Credential{
			rsaCertificate.WithTrustAnchorID(id1),
			&rsaCertificate,
		},

		flags: []string{"-expect-selected-credential", "1"},
	})

	testCases = append(testCases, testCase{
		name: "TrustAnchors-TLS12-Client",
		config: Config{
			MaxVersion:            VersionTLS12,
			AvailableTrustAnchors: [][]byte{id1},
			Bugs: ProtocolBugs{
				AlwaysSendAvailableTrustAnchors: true,
			},
		},
		flags:              []string{"-requested-trust-anchors", trustAnchorListFlagValue(id1)},
		shouldFail:         true,
		expectedError:      ":UNEXPECTED_EXTENSION:",
		expectedLocalError: "remote error: unsupported extension",
	})
}
