// Copyright 2017 The BoringSSL Authors
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

//go:build ignore

package main

import (
	"bytes"
	"fmt"
	"os"
	"strings"
)

func allSpaces(s string) bool {
	return strings.IndexFunc(s, func(r rune) bool { return r != ' ' }) == -1
}

func isContinuation(s string, column int) bool {
	if len(s) < column+2 {
		return false
	}
	if !allSpaces(s[:column]) {
		return false
	}
	return s[column:column+2] == " *"
}

func indexFrom(s, sep string, idx int) int {
	ret := strings.Index(s[idx:], sep)
	if ret < 0 {
		return -1
	}
	return idx + ret
}

type lineGroup struct {

	column int
	lines  []string
}

func addLine(groups *[]lineGroup, line string, column int) {
	if len(*groups) == 0 || (*groups)[len(*groups)-1].column != column {
		*groups = append(*groups, lineGroup{column, nil})
	}
	(*groups)[len(*groups)-1].lines = append((*groups)[len(*groups)-1].lines, line)
}

func writeLine(out *bytes.Buffer, line string) {
	out.WriteString(line)
	out.WriteByte('\n')
}

func convertComments(path string, in []byte) []byte {
	lines := strings.Split(string(in), "\n")

	if len(lines) > 0 && len(lines[len(lines)-1]) == 0 {
		lines = lines[:len(lines)-1]
	}

	var groups []lineGroup

	for len(lines) > 0 {
		line := lines[0]
		lines = lines[1:]
		addLine(&groups, line, -1)
		if len(line) == 0 {
			break
		}
	}

	var inComment bool

	var comment []string

	var column int
	for len(lines) > 0 {
		line := lines[0]
		lines = lines[1:]

		var idx int
		if inComment {

			if comment != nil && !isContinuation(line, column) {
				for _, l := range comment {
					addLine(&groups, l, -1)
				}
				comment = nil
			}

			idx = strings.Index(line, "*/")
			if idx < 0 {
				if comment != nil {
					comment = append(comment, line)
				} else {
					addLine(&groups, line, -1)
				}
				continue
			}

			inComment = false
			if comment != nil {
				if idx == len(line)-2 {

					if idx >= column+2 {

						comment = append(comment, line[:idx])
					}
					for _, l := range comment {
						addLine(&groups, l, column)
					}
					comment = nil
					continue
				}

				for _, l := range comment {
					addLine(&groups, l, -1)
				}
				comment = nil
			}
			idx += 2
		}

		for {
			idx = indexFrom(line, "/*", idx)
			if idx < 0 {
				addLine(&groups, line, -1)
				break
			}

			endIdx := indexFrom(line, "*/", idx)
			if endIdx < 0 {

				inComment = true
				column = idx
				comment = []string{line}
				break
			}

			if endIdx != len(line)-2 {

				idx = endIdx + 2
				continue
			}

			addLine(&groups, line[:endIdx], idx)
			break
		}
	}

	var out bytes.Buffer
	var lineNo int
	for _, group := range groups {
		if group.column < 0 {
			for _, line := range group.lines {
				writeLine(&out, line)
			}
		} else {

			var adjust string
			for _, line := range group.lines {
				if !allSpaces(line[:group.column]) && line[group.column-1] != '(' {
					if line[group.column-1] != ' ' {
						if len(adjust) < 2 {
							adjust = "  "
						}
					} else if line[group.column-2] != ' ' {
						if len(adjust) < 1 {
							adjust = " "
						}
					}
				}
			}

			for i, line := range group.lines {

				if i == 0 && allSpaces(line[:group.column]) && len(line) == group.column+2 {
					continue
				}
				newLine := fmt.Sprintf("%s%s//%s", line[:group.column], adjust, strings.TrimRight(line[group.column+2:], " "))
				if len(newLine) > 80 {
					fmt.Fprintf(os.Stderr, "%s:%d: Line is now longer than 80 characters\n", path, lineNo+i+1)
				}
				writeLine(&out, newLine)
			}

		}
		lineNo += len(group.lines)
	}
	return out.Bytes()
}

func main() {
	for _, arg := range os.Args[1:] {
		in, err := os.ReadFile(arg)
		if err != nil {
			panic(err)
		}
		if err := os.WriteFile(arg, convertComments(arg, in), 0666); err != nil {
			panic(err)
		}
	}
}
