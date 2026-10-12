#!/usr/bin/env perl
# Copyright 2018 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file defines helper functions for crypto/test/abi_test.h on x86. See
# that header for details on how to use this.
#
# For convenience, this file is linked into libcrypto, where consuming builds
# already support architecture-specific sources. The static linker should drop
# this code in non-test binaries. This includes a shared library build of
# libcrypto, provided --gc-sections (ELF), -dead_strip (Mac), or equivalent is
# used.
#
# References:
#
# SysV ABI: https://uclibc.org/docs/psABI-i386.pdf
# Win32 ABI: https://docs.microsoft.com/en-us/cpp/cpp/argument-passing-and-naming-conventions?view=vs-2017

use strict;

$0 =~ m/(.*[\/\\])[^\/\\]+$/;
my $dir = $1;
push(@INC, "${dir}", "${dir}../../perlasm");
require "x86asm.pl";

my $output = pop;
open STDOUT, ">$output";

&asm_init($ARGV[0]);

                                                                             
                                                                             
                                            
                                                                     
                                                                 
                                           
&function_begin("abi_test_trampoline")
	                                                                   
	                                                                           
	                          
	&mov("ecx", &wparam(1));
	&mov("esi", &DWP(4*0, "ecx"));
	&mov("edi", &DWP(4*1, "ecx"));
	&mov("ebx", &DWP(4*2, "ecx"));
	&mov("ebp", &DWP(4*3, "ecx"));

	                                                                        
	                                                                        
	                                                   
	&stack_push(11);

	                           
	&mov("eax", &wparam(2));
	&xor("ecx", "ecx");
&set_label("loop");
	&cmp("ecx", &wparam(3));
	&jae(&label("loop_done"));
	&mov("edx", &DWP(0, "eax", "ecx", 4));
	&mov(&DWP(0, "esp", "ecx", 4), "edx");
	&add("ecx", 1);
	&jmp(&label("loop"));

&set_label("loop_done");
	&call_ptr(&wparam(0));

	&stack_pop(11);

	                                   
	&mov("ecx", &wparam(1));
	&mov(&DWP(4*0, "ecx"), "esi");
	&mov(&DWP(4*1, "ecx"), "edi");
	&mov(&DWP(4*2, "ecx"), "ebx");
	&mov(&DWP(4*3, "ecx"), "ebp");
&function_end("abi_test_trampoline")

                                                                              
                                                                 
                                                  
&function_begin_B("abi_test_get_and_clear_direction_flag");
	&pushf();
	&pop("eax");
	&and("eax", 0x400);
	&shr("eax", 10);
	&cld();
	&ret();
&function_end_B("abi_test_get_and_clear_direction_flag");

                                                      
                                         
&function_begin_B("abi_test_set_direction_flag");
	&std();
	&ret();
&function_end_B("abi_test_set_direction_flag");

                                                                             
                            
foreach ("eax", "ebx", "ecx", "edx", "edi", "esi", "ebp") {
&function_begin_B("abi_test_clobber_$_");
	&xor($_, $_);
	&ret();
&function_end_B("abi_test_clobber_$_");
}
foreach (0..7) {
&function_begin_B("abi_test_clobber_xmm$_");
	&pxor("xmm$_", "xmm$_");
	&ret();
&function_end_B("abi_test_clobber_xmm$_");
}

&asm_finish();

close STDOUT or die "error closing STDOUT: $!";
