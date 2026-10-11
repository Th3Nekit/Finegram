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

func addKeyUpdateTests() {

	testCases = append(testCases, testCase{
		name: "KeyUpdate-ToClient",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		sendKeyUpdates:   10,
		keyUpdateRequest: keyUpdateNotRequested,
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "KeyUpdate-ToServer",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		sendKeyUpdates:   10,
		keyUpdateRequest: keyUpdateNotRequested,
	})
	testCases = append(testCases, testCase{
		name: "KeyUpdate-FromClient",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		expectUnsolicitedKeyUpdate: true,
		flags:                      []string{"-key-update"},
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "KeyUpdate-FromServer",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		expectUnsolicitedKeyUpdate: true,
		flags:                      []string{"-key-update"},
	})
	testCases = append(testCases, testCase{
		name: "KeyUpdate-InvalidRequestMode",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		sendKeyUpdates:   1,
		keyUpdateRequest: 42,
		shouldFail:       true,
		expectedError:    ":DECODE_ERROR:",
	})
	testCases = append(testCases, testCase{

		name: "KeyUpdate-Requested",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				RejectUnsolicitedKeyUpdate: true,
			},
		},

		sendKeyUpdates:   5,
		messageCount:     5,
		keyUpdateRequest: keyUpdateRequested,
	})
	testCases = append(testCases, testCase{

		name: "KeyUpdate-Requested-UnfinishedWrite",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				RejectUnsolicitedKeyUpdate: true,
			},
		},

		sendKeyUpdates:          5,
		messageCount:            5,
		keyUpdateRequest:        keyUpdateRequested,
		readWithUnfinishedWrite: true,
		flags:                   []string{"-async"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-ToClient-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},

		sendKeyUpdates:   10,
		keyUpdateRequest: keyUpdateNotRequested,
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "KeyUpdate-ToServer-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		sendKeyUpdates:   10,
		keyUpdateRequest: keyUpdateNotRequested,
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-ToClient-PacketLoss-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					if next[0].Type != typeKeyUpdate {
						c.WriteFlight(next)
						return
					}

					c.WriteFlight(next)
					ackTimeout := timeouts[0] / 4
					c.AdvanceClock(ackTimeout)
					c.ReadACK(c.InEpoch())

					msg := []byte("test")
					c.WriteAppData(c.OutEpoch()-1, msg)
					c.ReadAppData(c.InEpoch(), expectedReply(msg))

					c.WriteFlight(next)
					c.AdvanceClock(ackTimeout)
					c.ReadACK(c.InEpoch())

					c.WriteAppData(c.OutEpoch(), msg)
					c.ReadAppData(c.InEpoch(), expectedReply(msg))

					c.WriteAppData(c.OutEpoch()-1, msg)
					c.ReadAppData(c.InEpoch(), expectedReply(msg))

					c.WriteFlight(next)
					c.AdvanceClock(ackTimeout)
					c.ReadACK(c.InEpoch())

					c.AdvanceClock(dtlsPrevEpochExpiration)
					f := next[0].Fragment(0, len(next[0].Data))
					f.ShouldDiscard = true
					c.WriteFragments([]DTLSFragment{f})
					c.WriteAppData(c.OutEpoch()-1, msg)
				},
			},
		},
		sendKeyUpdates:   10,
		keyUpdateRequest: keyUpdateNotRequested,
		flags:            []string{"-async"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-FromClient-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		shimSendsKeyUpdateBeforeRead: true,

		messageCount: 10,
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "KeyUpdate-FromServer-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		shimSendsKeyUpdateBeforeRead: true,

		messageCount: 10,

		flags: []string{"-no-ticket"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-DeferredSend-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,

			ClientAuth: RequireAnyClientCert,
			Bugs: ProtocolBugs{
				MaxPacketLength: 512,
				ACKFlightDTLS: func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
					if received[len(received)-1].Type != typeFinished {
						c.WriteACK(c.OutEpoch(), records)
						return
					}

					if len(records) <= 1 {
						panic("shim sent Finished flight in one record")
					}

					msg := []byte("test")
					for i := 0; i < 10; i++ {
						c.WriteAppData(c.OutEpoch(), msg)
						c.ReadAppData(c.InEpoch(), expectedReply(msg))
					}

					c.WriteACK(c.OutEpoch(), records[:1])

					for i := 0; i < 10; i++ {
						c.WriteAppData(c.OutEpoch(), msg)
						c.ReadAppData(c.InEpoch(), expectedReply(msg))
					}

					c.WriteACK(c.OutEpoch(), records[1:])

				},
			},
		},
		shimCertificate:              &rsaChainCertificate,
		shimSendsKeyUpdateBeforeRead: true,
		flags:                        []string{"-mtu", "512"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-WaitForACK-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				MaxPacketLength: 512,
				ACKFlightDTLS: func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
					if received[0].Type != typeKeyUpdate {
						c.WriteACK(c.OutEpoch(), records)
						return
					}

					msg := []byte("test")
					for i := 0; i < 10; i++ {
						c.WriteAppData(c.OutEpoch(), msg)
						c.ReadAppData(c.InEpoch()-1, expectedReply(msg))
					}

					c.WriteACK(c.OutEpoch(), records)

				},
			},
		},
		shimSendsKeyUpdateBeforeRead: true,
	})

	fixKeyUpdateReply := func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
		c.WriteACK(c.OutEpoch(), records)
		if received[0].Type != typeKeyUpdate {
			return
		}

		msg := makeTestMessage(int(received[0].Sequence)-2, 32)
		c.ReadAppData(c.InEpoch()-1, expectedReply(msg))
		c.WriteAppData(c.OutEpoch(), msg)
	}
	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-Requested-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				RejectUnsolicitedKeyUpdate: true,
				ACKFlightDTLS:              fixKeyUpdateReply,
			},
		},

		sendKeyUpdates:   5,
		messageLen:       32,
		messageCount:     5,
		keyUpdateRequest: keyUpdateRequested,
	})

	mergeNewSessionTicketAndKeyUpdate := func(f WriteFlightFunc) WriteFlightFunc {
		return func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {

			if next[0].Type == typeKeyUpdate {
				panic("key update should have been merged into NewSessionTicket")
			}
			if next[0].Type != typeNewSessionTicket {
				c.WriteFlight(next)
				return
			}
			if next[0].Type == typeNewSessionTicket && next[len(next)-1].Type != typeKeyUpdate {
				c.MergeIntoNextFlight()
				return
			}

			f(c, prev, received, next, records)
		}
	}

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-ProcessInOrder-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: mergeNewSessionTicketAndKeyUpdate(func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {

					keyUpdate := next[len(next)-1]
					c.WriteFlight([]DTLSMessage{keyUpdate})
					ackTimeout := timeouts[0] / 4
					c.AdvanceClock(ackTimeout)
					c.ReadACK(c.InEpoch())

					msg1, msg2 := []byte("aaaa"), []byte("bbbb")
					c.WriteAppData(c.OutEpoch(), msg1)
					c.AdvanceClock(0)

					c.WriteAppData(c.OutEpoch()-1, msg2)
					c.ReadAppData(c.InEpoch(), expectedReply(msg2))

					c.WriteFlight(next[:len(next)-1])
					c.AdvanceClock(ackTimeout)
					c.ReadACK(c.InEpoch())

					c.WriteAppData(c.OutEpoch(), msg1)
					c.ReadAppData(c.InEpoch(), expectedReply(msg1))
				}),
			},
		},
		sendKeyUpdates:   1,
		keyUpdateRequest: keyUpdateNotRequested,
		flags:            []string{"-async"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-ExtraMessage-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: mergeNewSessionTicketAndKeyUpdate(func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					extra := next[0]
					extra.Sequence = next[len(next)-1].Sequence + 1
					next = append(slices.Clip(next), extra)
					c.WriteFlight(next)
				}),
			},
		},
		sendKeyUpdates:     1,
		keyUpdateRequest:   keyUpdateNotRequested,
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-ExtraMessageBuffered-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: mergeNewSessionTicketAndKeyUpdate(func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {

					extra := next[0]
					extra.Sequence = next[len(next)-1].Sequence + 1
					c.WriteFlight([]DTLSMessage{extra})

					c.WriteFlight(next)
				}),
			},
		},
		sendKeyUpdates:     1,
		keyUpdateRequest:   keyUpdateNotRequested,
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	const maxClientKeyUpdates = 0xffff - 3

	writeFlightKeyUpdate := func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
		if next[0].Type == typeKeyUpdate {

			msg := []byte("test")
			c.WriteAppData(c.OutEpoch()-1, msg)
			c.ReadAppData(c.InEpoch(), expectedReply(msg))
		}
		c.WriteFlight(next)
	}
	testCases = append(testCases, testCase{
		testType: serverTest,
		protocol: dtls,
		name:     "KeyUpdate-MaxReadEpoch-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AllowEpochOverflow: true,
				WriteFlightDTLS:    writeFlightKeyUpdate,
			},
		},

		flags:            []string{"-no-ticket"},
		sendKeyUpdates:   maxClientKeyUpdates,
		keyUpdateRequest: keyUpdateNotRequested,
	})
	testCases = append(testCases, testCase{
		testType: serverTest,
		protocol: dtls,
		name:     "KeyUpdate-ReadEpochOverflow-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{
				AllowEpochOverflow: true,
				WriteFlightDTLS:    writeFlightKeyUpdate,
			},
		},

		flags:              []string{"-no-ticket"},
		sendKeyUpdates:     maxClientKeyUpdates + 1,
		keyUpdateRequest:   keyUpdateNotRequested,
		shouldFail:         true,
		expectedError:      ":TOO_MANY_KEY_UPDATES:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-MaxWriteEpoch-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		shimSendsKeyUpdateBeforeRead: true,
		messageCount:                 maxClientKeyUpdates - 1,
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-WriteEpochOverflow-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
			Bugs: ProtocolBugs{

				AllowEpochOverflow: true,
			},
		},
		shimSendsKeyUpdateBeforeRead: true,
		messageCount:                 maxClientKeyUpdates,
		shouldFail:                   true,
		expectedError:                ":TOO_MANY_KEY_UPDATES:",
	})

	const maxServerKeyUpdates = 0xffff - 5

	testCases = append(testCases, testCase{
		protocol: dtls,
		name:     "KeyUpdate-ReadMessageOverflow-DTLS",
		config: Config{
			MaxVersion:             VersionTLS13,
			SessionTicketsDisabled: true,
			Bugs: ProtocolBugs{
				AllowEpochOverflow: true,
				WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					writeFlightKeyUpdate(c, prev, received, next, records)
					if next[0].Type == typeKeyUpdate && next[0].Sequence == 0xffff {

						c.WriteFlight([]DTLSMessage{{Epoch: c.OutEpoch(), Sequence: 0, Type: typeKeyUpdate, Data: []byte("INVALID")}})
						c.ExpectNextTimeout(timeouts[0] / 4)
						c.AdvanceClock(timeouts[0] / 4)
						c.ReadACK(c.InEpoch())
					}
				},
			},
		},
		sendKeyUpdates:   maxServerKeyUpdates + 1,
		keyUpdateRequest: keyUpdateNotRequested,
		flags:            []string{"-async", "-expect-no-session"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "KeyUpdate-MaxWriteMessage-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		shimSendsKeyUpdateBeforeRead: true,
		messageCount:                 maxServerKeyUpdates,

		flags: []string{"-no-ticket"},
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "KeyUpdate-WriteMessageOverflow-DTLS",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		shimSendsKeyUpdateBeforeRead: true,
		messageCount:                 maxServerKeyUpdates + 1,
		shouldFail:                   true,
		expectedError:                ":overflow:",

		flags: []string{"-no-ticket"},
	})
}
