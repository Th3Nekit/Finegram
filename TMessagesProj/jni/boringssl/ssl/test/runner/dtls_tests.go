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
	"slices"
	"strconv"
	"time"
)

func addDTLSReplayTests() {
	for _, vers := range allVersions(dtls) {

		testCases = append(testCases, testCase{
			protocol: dtls,
			name:     "DTLS-Replay-" + vers.name,
			config: Config{
				MaxVersion: vers.version,
			},
			messageCount: 200,
			replayWrites: true,
		})

		testCases = append(testCases, testCase{
			protocol: dtls,
			name:     "DTLS-Replay-LargeGaps-" + vers.name,
			config: Config{
				MaxVersion: vers.version,
				Bugs: ProtocolBugs{
					SequenceNumberMapping: func(in uint64) uint64 {
						return in * 1023
					},
				},
			},
			messageCount: 200,
			replayWrites: true,
		})

		testCases = append(testCases, testCase{
			protocol: dtls,
			name:     "DTLS-Replay-NonMonotonic-" + vers.name,
			config: Config{
				MaxVersion: vers.version,
				Bugs: ProtocolBugs{
					SequenceNumberMapping: func(in uint64) uint64 {

						return in ^ 255
					},
				},
			},

			messageCount: 500,
			replayWrites: true,
		})
	}
}

var timeouts = []time.Duration{
	400 * time.Millisecond,
	800 * time.Millisecond,
	1600 * time.Millisecond,
	3200 * time.Millisecond,
	6400 * time.Millisecond,
	12800 * time.Millisecond,
	25600 * time.Millisecond,
	51200 * time.Millisecond,
	60 * time.Second,
	60 * time.Second,
	60 * time.Second,
	60 * time.Second,
	60 * time.Second,
}

var shortTimeouts = []time.Duration{
	250 * time.Millisecond,
	500 * time.Millisecond,
	1 * time.Second,
	2 * time.Second,
	4 * time.Second,
	8 * time.Second,
	16 * time.Second,
	32 * time.Second,
	60 * time.Second,
	60 * time.Second,
	60 * time.Second,
	60 * time.Second,
	60 * time.Second,
}

const dtlsPrevEpochExpiration = 4*time.Minute + 1*time.Second

func addDTLSRetransmitTests() {
	for _, shortTimeout := range []bool{false, true} {
		for _, vers := range allVersions(dtls) {
			suffix := "-" + vers.name
			flags := []string{"-async"}
			useTimeouts := timeouts
			if shortTimeout {
				suffix += "-Short"
				flags = append(flags, "-initial-timeout-duration-ms", "250")
				useTimeouts = shortTimeouts
			}

			handleNewSessionTicket := func(f ACKFlightFunc) ACKFlightFunc {
				if vers.version < VersionTLS13 {
					return f
				}
				return func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {

					if received[0].Type == typeNewSessionTicket && len(received) < 2 {
						c.MergeIntoNextFlight()
						return
					}

					testMessage := makeTestMessage(0, 32)
					if received[0].Type == typeNewSessionTicket {
						c.ReadAppData(c.InEpoch(), expectedReply(testMessage))
					}

					f(c, prev, received, records)

					if received[0].Type == typeNewSessionTicket {
						c.WriteAppData(c.OutEpoch(), testMessage)
					}
				}
			}

			writeFlightBasic := func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
				if len(received) > 0 {

					for _, t := range useTimeouts[:len(useTimeouts)-1] {
						c.ExpectNextTimeout(t)
						c.AdvanceClock(t)
						c.ReadRetransmit()
					}
					c.ExpectNextTimeout(useTimeouts[len(useTimeouts)-1])
				}

				c.WriteFlight(next)
			}
			ackFlightBasic := handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
				if vers.version >= VersionTLS13 {

					for _, t := range useTimeouts[:len(useTimeouts)-1] {
						c.ExpectNextTimeout(t)
						c.AdvanceClock(t)
						c.ReadRetransmit()
					}
					c.ExpectNextTimeout(useTimeouts[len(useTimeouts)-1])

					c.WriteACK(c.OutEpoch(), records)
					return
				}

				for i := 0; i < 5; i++ {
					c.WriteFlight(prev)
					c.ReadRetransmit()
				}
			})
			testCases = append(testCases, testCase{
				protocol: dtls,
				name:     "DTLS-Retransmit-Client-Basic" + suffix,
				config: Config{
					MaxVersion: vers.version,
					Bugs: ProtocolBugs{
						WriteFlightDTLS: writeFlightBasic,
						ACKFlightDTLS:   ackFlightBasic,
					},
				},
				resumeSession: true,
				flags:         flags,
			})
			testCases = append(testCases, testCase{
				protocol: dtls,
				testType: serverTest,
				name:     "DTLS-Retransmit-Server-Basic" + suffix,
				config: Config{
					MaxVersion: vers.version,
					Bugs: ProtocolBugs{
						WriteFlightDTLS: writeFlightBasic,
						ACKFlightDTLS:   ackFlightBasic,
					},
				},
				resumeSession: true,
				flags:         flags,
			})

			if vers.version <= VersionTLS12 {

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-PartialProgress" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {

								msg := next[0]
								split := len(msg.Data) / 2
								c.WriteFragments([]DTLSFragment{msg.Fragment(0, split)})

								c.AdvanceClock(useTimeouts[0])
								c.ReadRetransmit()

								rest := []DTLSFragment{msg.Fragment(split, len(msg.Data)-split)}
								for _, m := range next[1:] {
									rest = append(rest, m.Fragment(0, len(m.Data)))
								}
								c.WriteFragments(rest)
							},
						},
					},
					flags: flags,
				})
			} else {

				testCases = append(testCases, testCase{
					testType: serverTest,
					protocol: dtls,
					name:     "DTLS-Retransmit-PartialProgress-Server" + suffix,
					config: Config{
						MaxVersion:    vers.version,
						DefaultCurves: []CurveID{},
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) == 0 && next[0].Type == typeClientHello {

									c.WriteFlight(next)
									return
								}

								msg := next[0]
								split := len(msg.Data) / 2
								c.WriteFragments([]DTLSFragment{msg.Fragment(0, split)})

								c.ExpectNextTimeout(useTimeouts[0] / 4)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(c.InEpoch())

								c.ExpectNoNextTimeout()
								for _, t := range useTimeouts {
									c.AdvanceClock(t)
								}

								rest := []DTLSFragment{msg.Fragment(split, len(msg.Data)-split)}
								for _, m := range next[1:] {
									rest = append(rest, m.Fragment(0, len(m.Data)))
								}
								c.WriteFragments(rest)
							},
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-PartialProgress-Client" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								msg := next[0]
								if msg.Type != typeServerHello {

									c.WriteFlight(next)
									return
								}

								split := len(msg.Data) / 2
								c.WriteFragments([]DTLSFragment{msg.Fragment(0, split)})

								c.ExpectNextTimeout(useTimeouts[0])
								c.AdvanceClock(useTimeouts[0])
								c.ReadRetransmit()

								c.WriteFragments([]DTLSFragment{msg.Fragment(split, len(msg.Data)-split)})
								c.ExpectNextTimeout(useTimeouts[1])
								c.AdvanceClock(useTimeouts[1])
								c.ReadRetransmit()

								c.WriteFragments([]DTLSFragment{next[1].Fragment(0, len(next[1].Data))})

								c.AdvanceClock(useTimeouts[2] / 4)
								c.ReadACK(uint16(encryptionHandshake))

								c.ExpectNoNextTimeout()
								for _, t := range useTimeouts[2:] {
									c.AdvanceClock(t)
								}

								var rest []DTLSFragment
								for _, m := range next[2:] {
									rest = append(rest, m.Fragment(0, len(m.Data)))
								}
								c.WriteFragments(rest)
							},
						},
					},
					flags: flags,
				})
			}

			testCases = append(testCases, testCase{
				protocol: dtls,
				name:     "DTLS-Retransmit-Timeout" + suffix,
				config: Config{
					MaxVersion: vers.version,
					Bugs: ProtocolBugs{
						WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
							for _, t := range useTimeouts[:len(useTimeouts)-1] {
								c.ExpectNextTimeout(t)
								c.AdvanceClock(t)
								c.ReadRetransmit()
							}
							c.ExpectNextTimeout(useTimeouts[len(useTimeouts)-1])
							c.AdvanceClock(useTimeouts[len(useTimeouts)-1])

						},
					},
				},
				resumeSession: true,
				flags:         flags,
				shouldFail:    true,
				expectedError: ":READ_TIMEOUT_EXPIRED:",
			})

			testCases = append(testCases, testCase{
				protocol: dtls,
				name:     "DTLS-Retransmit-Fudge" + suffix,
				config: Config{
					MaxVersion: vers.version,
					Bugs: ProtocolBugs{
						WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
							if len(received) > 0 {
								c.ExpectNextTimeout(useTimeouts[0])
								c.AdvanceClock(useTimeouts[0] - 10*time.Millisecond)
								c.ReadRetransmit()
							}
							c.WriteFlight(next)
						},
					},
				},
				resumeSession: true,
				flags:         flags,
			})

			testCases = append(testCases, testCase{
				protocol: dtls,
				name:     "DTLS-Retransmit-ChangeMTU" + suffix,
				config: Config{
					MaxVersion: vers.version,

					ClientAuth: RequireAnyClientCert,
					Bugs: ProtocolBugs{
						WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
							for i, mtu := range []int{300, 301, 302, 303, 299, 298, 297} {
								c.SetMTU(mtu)
								c.AdvanceClock(useTimeouts[i])
								c.ReadRetransmit()
							}
							c.WriteFlight(next)
						},
					},
				},
				shimCertificate: &rsaChainCertificate,
				flags:           flags,
			})

			if vers.version >= VersionTLS13 {

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKEverything" + suffix,
					config: Config{
						MaxVersion:       vers.version,
						Credential:       &rsaChainCertificate,
						CurvePreferences: []CurveID{CurveX25519MLKEM768},
						DefaultCurves:    []CurveID{},
						Bugs: ProtocolBugs{

							MaxPacketLength:          512,
							MaxHandshakeRecordLength: 512,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									ackEpoch := received[len(received)-1].Epoch
									c.ExpectNextTimeout(useTimeouts[0])
									c.WriteACK(ackEpoch, records)

									c.ExpectNoNextTimeout()
									for _, t := range useTimeouts {
										c.AdvanceClock(t)
									}
								}
								c.WriteFlight(next)
							},
							ACKFlightDTLS: handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								ackEpoch := received[len(received)-1].Epoch
								c.ExpectNextTimeout(useTimeouts[0])
								c.WriteACK(ackEpoch, records)

								c.ExpectNoNextTimeout()
								for _, t := range useTimeouts {
									c.AdvanceClock(t)
								}
							}),
							SequenceNumberMapping: func(in uint64) uint64 {

								return in ^ 63
							},
						},
					},
					shimCertificate: &rsaChainCertificate,
					flags: slices.Concat(flags, []string{
						"-mtu", "512",
						"-curves", strconv.Itoa(int(CurveX25519MLKEM768)),

						"-require-any-client-certificate",
					}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKReverse" + suffix,
					config: Config{
						MaxVersion:       vers.version,
						CurvePreferences: []CurveID{CurveX25519MLKEM768},
						DefaultCurves:    []CurveID{},
						Bugs: ProtocolBugs{
							MaxPacketLength: 512,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									ackEpoch := received[len(received)-1].Epoch
									for _, t := range useTimeouts[:len(useTimeouts)-1] {
										if len(records) > 0 {
											c.WriteACK(ackEpoch, []DTLSRecordNumberInfo{records[len(records)-1]})
										}
										c.AdvanceClock(t)
										records = c.ReadRetransmit()
									}
								}
								c.WriteFlight(next)
							},
							ACKFlightDTLS: handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								ackEpoch := received[len(received)-1].Epoch
								for _, t := range useTimeouts[:len(useTimeouts)-1] {
									if len(records) > 0 {
										c.WriteACK(ackEpoch, []DTLSRecordNumberInfo{records[len(records)-1]})
									}
									c.AdvanceClock(t)
									records = c.ReadRetransmit()
								}
							}),
						},
					},
					shimCertificate: &rsaChainCertificate,
					flags:           slices.Concat(flags, []string{"-mtu", "512", "-curves", strconv.Itoa(int(CurveX25519MLKEM768))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKForwards" + suffix,
					config: Config{
						MaxVersion:       vers.version,
						CurvePreferences: []CurveID{CurveX25519MLKEM768},
						DefaultCurves:    []CurveID{},
						Bugs: ProtocolBugs{
							MaxPacketLength: 512,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									ackEpoch := received[len(received)-1].Epoch
									for _, t := range useTimeouts[:len(useTimeouts)-1] {
										if len(records) > 0 {
											c.WriteACK(ackEpoch, []DTLSRecordNumberInfo{records[0]})
										}
										c.AdvanceClock(t)
										records = c.ReadRetransmit()
									}
								}
								c.WriteFlight(next)
							},
							ACKFlightDTLS: handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								ackEpoch := received[len(received)-1].Epoch
								for _, t := range useTimeouts[:len(useTimeouts)-1] {
									if len(records) > 0 {
										c.WriteACK(ackEpoch, []DTLSRecordNumberInfo{records[0]})
									}
									c.AdvanceClock(t)
									records = c.ReadRetransmit()
								}
							}),
						},
					},
					shimCertificate: &rsaChainCertificate,
					flags:           slices.Concat(flags, []string{"-mtu", "512", "-curves", strconv.Itoa(int(CurveX25519MLKEM768))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKIterate" + suffix,
					config: Config{
						MaxVersion:       vers.version,
						CurvePreferences: []CurveID{CurveX25519MLKEM768},
						DefaultCurves:    []CurveID{},
						Bugs: ProtocolBugs{
							MaxPacketLength: 512,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									ackEpoch := received[len(received)-1].Epoch
									for i, t := range useTimeouts[:len(useTimeouts)-1] {
										if len(records) > 0 {
											ack := make([]DTLSRecordNumberInfo, 0, (len(records)+2)/3)
											for i := 0; i < len(records); i += 3 {
												ack = append(ack, records[i])
											}
											c.WriteACK(ackEpoch, ack)
										}

										c.SetMTU(512 + i)
										c.AdvanceClock(t)
										records = c.ReadRetransmit()
									}
								}
								c.WriteFlight(next)
							},
							ACKFlightDTLS: handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								ackEpoch := received[len(received)-1].Epoch
								for _, t := range useTimeouts[:len(useTimeouts)-1] {
									if len(records) > 0 {
										c.WriteACK(ackEpoch, []DTLSRecordNumberInfo{records[0]})
									}
									c.AdvanceClock(t)
									records = c.ReadRetransmit()
								}
							}),
						},
					},
					shimCertificate: &rsaChainCertificate,
					flags:           slices.Concat(flags, []string{"-mtu", "512", "-curves", strconv.Itoa(int(CurveX25519MLKEM768))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKDuplicate" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							SendHelloRetryRequestCookie: []byte("cookie"),
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									ackEpoch := received[len(received)-1].Epoch

									c.WriteACK(ackEpoch, records[:1])
									c.AdvanceClock(useTimeouts[0])
									c.ReadRetransmit()
									c.WriteACK(ackEpoch, records[:1])
									c.AdvanceClock(useTimeouts[1])
									c.ReadRetransmit()
								}
								c.WriteFlight(next)
							},
							ACKFlightDTLS: handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								ackEpoch := received[len(received)-1].Epoch

								c.WriteACK(ackEpoch, records[:1])
								c.AdvanceClock(useTimeouts[0])
								c.ReadRetransmit()
								c.WriteACK(ackEpoch, records[:1])
								c.AdvanceClock(useTimeouts[1])
								c.ReadRetransmit()

								c.WriteACK(ackEpoch, records)
							}),
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKMatchingEpoch" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									for _, t := range useTimeouts[:len(useTimeouts)-1] {
										if len(records) > 0 {
											c.WriteACK(uint16(records[0].Epoch), []DTLSRecordNumberInfo{records[0]})
										}
										c.AdvanceClock(t)
										records = c.ReadRetransmit()
									}
								}
								c.WriteFlight(next)
							},
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKBadEpoch" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) == 0 {

									c.WriteFlight(next)
								} else {

									c.WriteACK(0, records)
								}
							},
						},
					},
					flags:         flags,
					shouldFail:    true,
					expectedError: ":DECODE_ERROR:",
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKEpochOverflow" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) == 0 {

									c.WriteFlight(next)
								} else {
									r := records[0]
									r.Epoch += 1 << 63
									c.WriteACK(0, []DTLSRecordNumberInfo{r})
								}
							},
						},
					},
					flags:         flags,
					shouldFail:    true,
					expectedError: ":DECODE_ERROR:",
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKOldRecords" + suffix,
					config: Config{
						MaxVersion:       vers.version,
						CurvePreferences: []CurveID{CurveX25519MLKEM768},
						Bugs: ProtocolBugs{
							MaxPacketLength: 512,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {
									ackEpoch := received[len(received)-1].Epoch
									c.WriteACK(ackEpoch, records[len(records)/2:])
									c.AdvanceClock(useTimeouts[0])
									c.ReadRetransmit()
									c.WriteACK(ackEpoch, records[:len(records)/2])

									c.AdvanceClock(useTimeouts[1])
									c.AdvanceClock(useTimeouts[2])
									c.AdvanceClock(useTimeouts[3])
								}
								c.WriteFlight(next)
							},
						},
					},
					flags: slices.Concat(flags, []string{"-mtu", "512", "-curves", strconv.Itoa(int(CurveX25519MLKEM768))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKForgottenRecords" + suffix,
					config: Config{
						MaxVersion:       vers.version,
						CurvePreferences: []CurveID{CurveX25519MLKEM768},
						Bugs: ProtocolBugs{
							MaxPacketLength: 256,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) > 0 {

									for _, t := range useTimeouts[:len(useTimeouts)-2] {
										c.AdvanceClock(t)
										c.ReadRetransmit()
									}

									c.WriteACK(c.OutEpoch(), []DTLSRecordNumberInfo{{DTLSRecordNumber: records[0].DTLSRecordNumber}})
									c.AdvanceClock(useTimeouts[len(useTimeouts)-2])
									c.ReadRetransmit()
								}
								c.WriteFlight(next)
							},
						},
					},
					flags: slices.Concat(flags, []string{"-mtu", "256", "-curves", strconv.Itoa(int(CurveX25519MLKEM768))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKPreviousFlight" + suffix,
					config: Config{
						MaxVersion:    vers.version,
						DefaultCurves: []CurveID{},
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if next[len(next)-1].Type == typeFinished {

									c.WriteACK(c.OutEpoch(), []DTLSRecordNumberInfo{{DTLSRecordNumber: DTLSRecordNumber{Epoch: 0, Sequence: 0}}})
									c.AdvanceClock(useTimeouts[0])
									c.ReadRetransmit()
								}
								c.WriteFlight(next)
							},
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-DoNotACKDiscardedFragments" + suffix,
					config: Config{
						MaxVersion:    vers.version,
						DefaultCurves: []CurveID{},
						Bugs: ProtocolBugs{
							PackHandshakeFragments: 4096,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {

								for _, msg := range next {
									shouldDiscard := DTLSFragment{Epoch: msg.Epoch, Sequence: 1000, ShouldDiscard: true}
									c.WriteFragments([]DTLSFragment{shouldDiscard, msg.Fragment(0, len(msg.Data))})

									c.ExpectNextTimeout(useTimeouts[0])
								}
							},
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					testType: serverTest,
					name:     "DTLS-Retransmit-Server-ACKFinishedAfterAppData" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{

							SkipImplicitACKRead: true,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if next[len(next)-1].Type != typeFinished {
									c.WriteFlight(next)
									return
								}

								c.WriteFlight(next)
								c.ReadACK(c.InEpoch())

								msg := []byte("hello")
								c.WriteAppData(c.OutEpoch(), msg)
								c.ReadAppData(c.InEpoch(), expectedReply(msg))

								c.WriteFlight(next)
								ackTimeout := useTimeouts[0] / 4
								c.AdvanceClock(ackTimeout)
								c.ReadACK(c.InEpoch())

								c.WriteFragments([]DTLSFragment{next[0].Fragment(0, 1)})
								c.WriteFragments([]DTLSFragment{next[0].Fragment(1, 1)})
								c.AdvanceClock(ackTimeout)
								c.ReadACK(c.InEpoch())

								c.AdvanceClock(dtlsPrevEpochExpiration)
								c.WriteFlight(next)
							},
						},
					},

					flags: slices.Concat(flags, []string{"-no-ticket"}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-Client" + suffix,
					config: Config{
						MaxVersion: vers.version,

						ClientAuth: RequireAnyClientCert,
						Bugs: ProtocolBugs{
							SendHelloRetryRequestCookie: []byte("cookie"),
							MaxPacketLength:             512,
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if len(received) == 0 || received[0].Type != typeClientHello {

									c.WriteFlight(next)
									return
								}

								first := records[0]
								if len(prev) == 0 {

									first.MessageStartSequence = 0
									first.MessageStartOffset = 0
									first.MessageEndSequence = 0
									first.MessageEndOffset = 0
								}
								c.WriteACK(0, []DTLSRecordNumberInfo{first})
								c.AdvanceClock(useTimeouts[0])
								c.ReadRetransmit()
								c.WriteFlight(next)
							},
							ACKFlightDTLS: func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {

								msg := []byte("hello")
								c.WriteAppData(c.OutEpoch(), msg)
								c.ReadAppData(c.InEpoch(), expectedReply(msg))

								c.AdvanceClock(useTimeouts[0])
								c.ReadRetransmit()

								c.WriteAppData(c.OutEpoch(), msg)
								c.ReadAppData(c.InEpoch(), expectedReply(msg))

								c.WriteACK(c.OutEpoch(), records[len(records)/3:2*len(records)/3])
								c.AdvanceClock(useTimeouts[1])
								records = c.ReadRetransmit()

								c.WriteACK(c.OutEpoch(), records)
								for _, t := range useTimeouts[2:] {
									c.AdvanceClock(t)
								}
							},
						},
					},
					shimCertificate: &rsaChainCertificate,
					flags:           slices.Concat(flags, []string{"-mtu", "512", "-curves", strconv.Itoa(int(CurveX25519MLKEM768))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-Client-FinishedTimeout" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							ACKFlightDTLS: func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								for _, t := range useTimeouts[:len(useTimeouts)-1] {
									c.AdvanceClock(t)
									c.ReadRetransmit()
								}
								c.AdvanceClock(useTimeouts[len(useTimeouts)-1])
							},
						},
					},
					flags:         flags,
					shouldFail:    true,
					expectedError: ":READ_TIMEOUT_EXPIRED:",
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-Client-NoImplictACKFinished" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							ACKFlightDTLS: func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {

								c.MergeIntoNextFlight()
							},
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if next[0].Type != typeNewSessionTicket {
									c.WriteFlight(next)
									return
								}
								if len(received) == 0 || received[0].Type != typeFinished {
									panic("Finished should be merged with NewSessionTicket")
								}

								if next[len(next)-1].Type != typeKeyUpdate {
									c.MergeIntoNextFlight()
									return
								}

								c.WriteFlight(next)
								ackTimeout := useTimeouts[0] / 4
								c.AdvanceClock(ackTimeout)
								c.ReadACK(c.InEpoch())

								c.AdvanceClock(useTimeouts[0] - ackTimeout)
								c.ReadRetransmit()

								msg := []byte("test")
								c.WriteAppData(c.OutEpoch()-1, msg)
								c.ReadAppData(c.InEpoch(), expectedReply(msg))

								c.AdvanceClock(useTimeouts[1])
								c.ReadRetransmit()

								c.WriteAppData(c.OutEpoch(), msg)
								c.ReadAppData(c.InEpoch(), expectedReply(msg))

								c.AdvanceClock(useTimeouts[2])
								c.ReadRetransmit()

								c.WriteACK(c.OutEpoch(), records)
								c.ExpectNoNextTimeout()
							},
						},
					},
					sendKeyUpdates:   1,
					keyUpdateRequest: keyUpdateNotRequested,
					flags:            flags,
				})

				testCases = append(testCases, testCase{
					testType: serverTest,
					protocol: dtls,
					name:     "DTLS-Retransmit-Server-NewSessionTicketTimeout" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							ACKFlightDTLS: handleNewSessionTicket(func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
								if received[0].Type != typeNewSessionTicket {
									c.WriteACK(c.OutEpoch(), records)
									return
								}

								for _, t := range useTimeouts[:len(useTimeouts)-1] {
									c.AdvanceClock(t)
									c.ReadRetransmit()
								}
								c.AdvanceClock(useTimeouts[len(useTimeouts)-1])
							}),
						},
					},
					flags:         flags,
					shouldFail:    true,
					expectedError: ":READ_TIMEOUT_EXPIRED:",
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-SlowReplyGeneration" + suffix,
					config: Config{
						MaxVersion: vers.version,
						ClientAuth: RequireAnyClientCert,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								c.WriteFlight(next)
								if next[0].Type == typeServerHello {

									c.ReadACK(c.InEpoch())
								}
							},
						},
					},
					shimCertificate: &rsaCertificate,

					flags: slices.Concat(flags, []string{"-private-key-delay-ms", strconv.Itoa(int(useTimeouts[0].Milliseconds()))}),
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-BothTimers" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{

							SendHelloRetryRequestCookie: []byte("cookie"),
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if next[0].Sequence == 0 || next[0].Type != typeServerHello {

									c.WriteFlight(next)
									return
								}

								c.ExpectNextTimeout(useTimeouts[0])

								c.WriteFragments([]DTLSFragment{prev[0].Fragment(0, 1)})

								c.ExpectNextTimeout(useTimeouts[0] / 4)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(0)

								c.ExpectNextTimeout(3 * useTimeouts[0] / 4)
								c.AdvanceClock(3 * useTimeouts[0] / 4)
								c.ReadRetransmit()

								c.ExpectNextTimeout(useTimeouts[1])

								c.WriteFragments([]DTLSFragment{prev[0].Fragment(0, 1)})
								c.ExpectNextTimeout(useTimeouts[1] / 4)

								c.AdvanceClock(useTimeouts[1])
								c.ReadACK(0)
								c.ReadRetransmit()

								c.WriteFlight(next)
							},
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-Client-ACKPostHandshake" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if next[0].Type != typeNewSessionTicket {
									c.WriteFlight(next)
									return
								}

								if len(next) != 2 {
									panic("unexpected message count")
								}

								first0, second0 := next[0].Split(len(next[0].Data) / 2)
								first1, second1 := next[1].Split(len(next[1].Data) / 2)
								c.WriteFragments([]DTLSFragment{first0})

								c.ExpectNextTimeout(useTimeouts[0] / 4)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(c.InEpoch())

								c.ExpectNoNextTimeout()

								c.WriteFragments([]DTLSFragment{first0, second1})

								c.ExpectNextTimeout(useTimeouts[0] / 4)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(c.InEpoch())
								c.ExpectNoNextTimeout()

								c.WriteFragments([]DTLSFragment{first1, second0})

								c.ExpectNextTimeout(useTimeouts[0] / 4)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(c.InEpoch())
								c.ExpectNoNextTimeout()
							},
						},
					},
					flags: flags,
				})

				testCases = append(testCases, testCase{
					protocol: dtls,
					name:     "DTLS-Retransmit-Client-ACKPostHandshakeTwice" + suffix,
					config: Config{
						MaxVersion: vers.version,
						Bugs: ProtocolBugs{
							WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
								if next[0].Type != typeNewSessionTicket {
									c.WriteFlight(next)
									return
								}

								if len(next) != 2 {
									panic("unexpected message count")
								}

								c.WriteFlight(next)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(c.InEpoch())
								c.ExpectNoNextTimeout()

								c.WriteFlight(next)
								c.AdvanceClock(useTimeouts[0] / 4)
								c.ReadACK(c.InEpoch())
								c.ExpectNoNextTimeout()
							},
						},
					},
					flags: flags,
				})
			}
		}
	}

	testCases = append(testCases, testCase{
		testType: serverTest,
		protocol: dtls,
		name:     "DTLS-RetransmitFinished-Fragmented",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				MaxHandshakeRecordLength: 2,
				ACKFlightDTLS: func(c *DTLSController, prev, received []DTLSMessage, records []DTLSRecordNumberInfo) {
					c.WriteFlight(prev)
					c.ReadRetransmit()
				},
			},
		},
		flags: []string{"-async"},
	})

	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: clientTest,
		name:     "DTLS-StrayRetransmitFinished-ClientFull",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					c.WriteFlight(next)
					for _, msg := range next {
						if msg.Type == typeFinished {
							c.WriteFlight([]DTLSMessage{msg})
						}
					}
				},
			},
		},
	})
	testCases = append(testCases, testCase{
		protocol: dtls,
		testType: serverTest,
		name:     "DTLS-StrayRetransmitFinished-ServerResume",
		config: Config{
			MaxVersion: VersionTLS12,
		},
		resumeConfig: &Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				WriteFlightDTLS: func(c *DTLSController, prev, received, next []DTLSMessage, records []DTLSRecordNumberInfo) {
					c.WriteFlight(next)
					for _, msg := range next {
						if msg.Type == typeFinished {
							c.WriteFlight([]DTLSMessage{msg})
						}
					}
				},
			},
		},
		resumeSession: true,
	})
}

func addDTLSReorderTests() {
	for _, vers := range allVersions(dtls) {
		testCases = append(testCases, testCase{
			protocol: dtls,
			name:     "ReorderHandshakeFragments-Small-DTLS-" + vers.name,
			config: Config{
				MaxVersion: vers.version,
				Bugs: ProtocolBugs{
					ReorderHandshakeFragments: true,

					MaxHandshakeRecordLength: 2,
				},
			},
		})
		testCases = append(testCases, testCase{
			protocol: dtls,
			name:     "ReorderHandshakeFragments-Large-DTLS-" + vers.name,
			config: Config{
				MaxVersion: vers.version,
				Bugs: ProtocolBugs{
					ReorderHandshakeFragments: true,

					MaxHandshakeRecordLength: 2048,
				},
			},
		})
		testCases = append(testCases, testCase{
			protocol: dtls,
			name:     "MixCompleteMessageWithFragments-DTLS-" + vers.name,
			config: Config{
				MaxVersion: vers.version,
				Bugs: ProtocolBugs{
					ReorderHandshakeFragments:       true,
					MixCompleteMessageWithFragments: true,
					MaxHandshakeRecordLength:        2,
				},
			},
		})
	}
}
