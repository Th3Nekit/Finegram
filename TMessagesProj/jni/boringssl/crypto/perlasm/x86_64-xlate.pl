#! /usr/bin/env perl
# Copyright 2005-2016 The OpenSSL Project Authors. All Rights Reserved.
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


# Ascetic x86_64 AT&T to MASM/NASM assembler translator by <appro>.
#
# Why AT&T to MASM and not vice versa? Several reasons. Because AT&T
# format is way easier to parse. Because it's simpler to "gear" from
# Unix ABI to Windows one [see cross-reference "card" at the end of
# file]. Because Linux targets were available first...
#
# In addition the script also "distills" code suitable for GNU
# assembler, so that it can be compiled with more rigid assemblers,
# such as Solaris /usr/ccs/bin/as.
#
# This translator is not designed to convert *arbitrary* assembler
# code from AT&T format to MASM one. It's designed to convert just
# enough to provide for dual-ABI OpenSSL modules development...
# There *are* limitations and you might have to modify your assembler
# code or this script to achieve the desired result...
#
# Currently recognized limitations:
#
# - can't use multiple ops per line;
#
# Dual-ABI styling rules.
#
# 1. Adhere to Unix register and stack layout [see cross-reference
#    ABI "card" at the end for explanation].
# 2. Forget about "red zone," stick to more traditional blended
#    stack frame allocation. If volatile storage is actually required
#    that is. If not, just leave the stack as is.
# 3. Functions tagged with ".type name,@function" get crafted with
#    unified Win64 prologue and epilogue automatically. If you want
#    to take care of ABI differences yourself, tag functions as
#    ".type name,@abi-omnipotent" instead.
# 4. To optimize the Win64 prologue you can specify number of input
#    arguments as ".type name,@function,N." Keep in mind that if N is
#    larger than 6, then you *have to* write "abi-omnipotent" code,
#    because >6 cases can't be addressed with unified prologue.
# 5. Name local labels as .L*, do *not* use dynamic labels such as 1:
#    (sorry about latter).
# 6. Don't use [or hand-code with .byte] "rep ret." "ret" mnemonic is
#    required to identify the spots, where to inject Win64 epilogue!
# 7. Stick to explicit ip-relative addressing. If you have to use
#    GOTPCREL addressing, stick to mov symbol@GOTPCREL(%rip),%r??.
#    Both are recognized and translated to proper Win64 addressing
#    modes.
#
# 8. In order to provide for structured exception handling unified
#    Win64 prologue copies %rsp value to %rax. For further details
#    see SEH paragraph at the end.
# 9. .init segment is allowed to contain calls to functions only.
# a. If function accepts more than 4 arguments *and* >4th argument
#    is declared as non 64-bit value, do clear its upper part.
#
# TODO(https://crbug.com/boringssl/259): The dual-ABI mechanism described here
# does not quite unwind correctly on Windows. The seh_directive logic below has
# the start of a new mechanism.


use strict;

my $flavour = shift;
my $output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

open STDOUT,">$output" || die "can't open $output: $!"
	if (defined($output));

my $gas=1;	$gas=0 if ($output =~ /\.asm$/);
my $elf=1;	$elf=0 if (!$gas);
my $apple=0;
my $win64=0;
my $prefix="";
my $decor=".L";

my $masmref=8 + 50727*2**-32;	                                
my $masm=0;
my $PTR=" PTR";

my $nasmref=2.03;
my $nasm=0;

if    ($flavour eq "mingw64")	{ $gas=1; $elf=0; $win64=1;
				                                         
				                                              
				                               
                                  die "mingw64 not supported";
				  $prefix=`echo __USER_LABEL_PREFIX__ | $ENV{CC} -E -P -`;
				  $prefix =~ s|\R$||;               
				}
elsif ($flavour eq "macosx")	{ $gas=1; $elf=0; $apple=1; $prefix="_"; $decor="L\$"; }
elsif ($flavour eq "masm")	{ $gas=0; $elf=0; $masm=$masmref; $win64=1; $decor="\$L\$"; }
elsif ($flavour eq "nasm")	{ $gas=0; $elf=0; $nasm=$nasmref; $win64=1; $decor="\$L\$"; $PTR=""; }
elsif (!$gas)			{ die "unknown flavour $flavour"; }

my $current_segment;
my $current_function;
my %globals;

{ package opcode;	                 
    sub re {
	my	($class, $line) = @_;
	my	$self = {};
	my	$ret;

	if ($$line =~ /^([a-z][a-z0-9]*)/i) {
	    bless $self,$class;
	    $self->{op} = $1;
	    $ret = $self;
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;

	    undef $self->{sz};
	    if ($self->{op} =~ /^(movz)x?([bw]).*/) {	                 
		$self->{op} = $1;
		$self->{sz} = $2;
	    } elsif ($self->{op} =~ /call|jmp|^rdrand$/) {
		$self->{sz} = "";
	    } elsif ($self->{op} =~ /^p/ && $' !~ /^(ush|op|insrw)/) {       
		$self->{sz} = "";
	    } elsif ($self->{op} =~ /^[vk]/) {                         
		$self->{sz} = "";
	    } elsif ($self->{op} =~ /mov[dq]/ && $$line =~ /%xmm/) {
		$self->{sz} = "";
	    } elsif ($self->{op} =~ /^or([qlwb])$/) {
		$self->{op} = "or";
		$self->{sz} = $1;
	    } elsif ($self->{op} =~ /([a-z]{3,})([qlwb])$/) {
		$self->{op} = $1;
		$self->{sz} = $2;
	    }
	}
	$ret;
    }
    sub size {
	my ($self, $sz) = @_;
	$self->{sz} = $sz if (defined($sz) && !defined($self->{sz}));
	$self->{sz};
    }
    sub out {
	my $self = shift;
	if ($gas) {
	    if ($self->{op} eq "movz") {	                 
		sprintf "%s%s%s",$self->{op},$self->{sz},shift;
	    } elsif ($self->{op} =~ /^set/) {
		"$self->{op}";
	    } elsif ($self->{op} eq "ret") {
		my $epilogue = "";
		if ($win64 && $current_function->{abi} eq "svr4") {
		    $epilogue = "movq	8(%rsp),%rdi\n\t" .
				"movq	16(%rsp),%rsi\n\t";
		}
	    	$epilogue . "ret";
	    } elsif ($self->{op} eq "call" && !$elf && $current_segment eq ".init") {
		".p2align\t3\n\t.quad";
	    } else {
		"$self->{op}$self->{sz}";
	    }
	} else {
	    $self->{op} =~ s/^movz/movzx/;
	    if ($self->{op} eq "ret") {
		$self->{op} = "";
		if ($win64 && $current_function->{abi} eq "svr4") {
		    $self->{op} = "mov	rdi,QWORD$PTR\[8+rsp\]\t;WIN64 epilogue\n\t".
				  "mov	rsi,QWORD$PTR\[16+rsp\]\n\t";
	    	}
		$self->{op} .= "ret";
	    } elsif ($self->{op} =~ /^(pop|push)f/) {
		$self->{op} .= $self->{sz};
	    } elsif ($self->{op} eq "call" && $current_segment eq ".CRT\$XCU") {
		$self->{op} = "\tDQ";
	    }
	    $self->{op};
	}
    }
    sub mnemonic {
	my ($self, $op) = @_;
	$self->{op}=$op if (defined($op));
	$self->{op};
    }
}
{ package const;	                                       
    sub re {
	my	($class, $line) = @_;
	my	$self = {};
	my	$ret;

	if ($$line =~ /^\$([^,]+)/) {
	    bless $self, $class;
	    $self->{value} = $1;
	    $ret = $self;
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;
	}
	$ret;
    }
    sub out {
    	my $self = shift;

	$self->{value} =~ s/\b(0b[0-1]+)/oct($1)/eig;
	if ($gas) {
	                                                          
	                       
	    my $value = $self->{value};
	    no warnings;                                                       
	    $value =~ s/(?<![\w\$\.])(0x?[0-9a-f]+)/oct($1)/egi;
	    if ($value =~ s/([0-9]+\s*[\*\/\%]\s*[0-9]+)/eval($1)/eg) {
		$self->{value} = $value;
	    }
	    sprintf "\$%s",$self->{value};
	} else {
	    my $value = $self->{value};
	    $value =~ s/0x([0-9a-f]+)/0$1h/ig if ($masm);
	    sprintf "%s",$value;
	}
    }
}
{ package ea;		                                                    

    my %szmap = (	b=>"BYTE$PTR",    w=>"WORD$PTR",
			l=>"DWORD$PTR",   d=>"DWORD$PTR",
			q=>"QWORD$PTR",   o=>"OWORD$PTR",
			x=>"XMMWORD$PTR", y=>"YMMWORD$PTR",
			z=>"ZMMWORD$PTR" ) if (!$gas);

    sub re {
	my	($class, $line, $opcode) = @_;
	my	$self = {};
	my	$ret;

	                                                    
	if ($$line =~ /^(\*?)([^\(,]*)\(([%\w,]+)\)((?:{[^}]+})*)/) {
	    bless $self, $class;
	    $self->{asterisk} = $1;
	    $self->{label} = $2;
	    ($self->{base},$self->{index},$self->{scale})=split(/,/,$3);
	    $self->{scale} = 1 if (!defined($self->{scale}));
	    $self->{opmask} = $4;
	    $ret = $self;
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;

	    if ($win64 && $self->{label} =~ s/\@GOTPCREL//) {
		die if ($opcode->mnemonic() ne "mov");
		$opcode->mnemonic("lea");
	    }
	    $self->{base}  =~ s/^%//;
	    $self->{index} =~ s/^%// if (defined($self->{index}));
	    $self->{opcode} = $opcode;
	}
	$ret;
    }
    sub size {}
    sub out {
	my ($self, $sz) = @_;

	$self->{label} =~ s/([_a-z][_a-z0-9]*)/$globals{$1} or $1/gei;
	$self->{label} =~ s/\.L/$decor/g;

	                                                          
	                                                       
	                                                            
	$self->{index} =~ s/^[er](.?[0-9xpi])[d]?$/r\1/;
	$self->{base}  =~ s/^[er](.?[0-9xpi])[d]?$/r\1/;

	                                                      
	                      
	use integer;
	$self->{label} =~ s/(?<![\w\$\.])(0x?[0-9a-f]+)/oct($1)/egi;
	$self->{label} =~ s/\b([0-9]+\s*[\*\/\%]\s*[0-9]+)\b/eval($1)/eg;

	                                                         
	                                                             
	if ((1<<31)<<1) {
	    $self->{label} =~ s/\b([0-9]+)\b/$1<<32>>32/eg;
	} else {
	    $self->{label} =~ s/\b([0-9]+)\b/$1>>0/eg;
	}

	                                                           
	                                                        
	if (!$self->{label} && $self->{index} && $self->{scale}==1 &&
	    $self->{base} =~ /(rbp|r13)/) {
		$self->{base} = $self->{index}; $self->{index} = $1;
	}

	if ($gas) {
	    $self->{label} =~ s/^___imp_/__imp__/   if ($flavour eq "mingw64");

	    if (defined($self->{index})) {
		sprintf "%s%s(%s,%%%s,%d)%s",
					$self->{asterisk},$self->{label},
					$self->{base}?"%$self->{base}":"",
					$self->{index},$self->{scale},
					$self->{opmask};
	    } else {
		sprintf "%s%s(%%%s)%s",	$self->{asterisk},$self->{label},
					$self->{base},$self->{opmask};
	    }
	} else {
	    $self->{label} =~ s/\./\$/g;
	    $self->{label} =~ s/(?<![\w\$\.])0x([0-9a-f]+)/0$1h/ig;
	    $self->{label} = "($self->{label})" if ($self->{label} =~ /[\*\+\-\/]/);

	    my $mnemonic = $self->{opcode}->mnemonic();
	    ($self->{asterisk})				&& ($sz="q") ||
	    ($mnemonic =~ /^v?mov([qd])$/)		&& ($sz=$1)  ||
	    ($mnemonic =~ /^v?pinsr([qdwb])$/)		&& ($sz=$1)  ||
	    ($mnemonic =~ /^vpbroadcast([qdwb])$/)	&& ($sz=$1)  ||
	    ($mnemonic =~ /^v(?!perm)[a-z]+[fi]128$/)	&& ($sz="x");

	    $self->{opmask}  =~ s/%(k[0-7])/$1/;

	    if (defined($self->{index})) {
		sprintf "%s[%s%s*%d%s]%s",$szmap{$sz},
					$self->{label}?"$self->{label}+":"",
					$self->{index},$self->{scale},
					$self->{base}?"+$self->{base}":"",
					$self->{opmask};
	    } elsif ($self->{base} eq "rip") {
		sprintf "%s[%s]",$szmap{$sz},$self->{label};
	    } else {
		sprintf "%s[%s%s]%s",	$szmap{$sz},
					$self->{label}?"$self->{label}+":"",
					$self->{base},$self->{opmask};
	    }
	}
    }
}
{ package register;	                                        
    sub re {
	my	($class, $line, $opcode) = @_;
	my	$self = {};
	my	$ret;

	                                                    
	if ($$line =~ /^(\*?)%(\w+)((?:{[^}]+})*)/) {
	    bless $self,$class;
	    $self->{asterisk} = $1;
	    $self->{value} = $2;
	    $self->{opmask} = $3;
	    $opcode->size($self->size());
	    $ret = $self;
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;
	}
	$ret;
    }
    sub size {
	my	$self = shift;
	my	$ret;

	if    ($self->{value} =~ /^r[\d]+b$/i)	{ $ret="b"; }
	elsif ($self->{value} =~ /^r[\d]+w$/i)	{ $ret="w"; }
	elsif ($self->{value} =~ /^r[\d]+d$/i)	{ $ret="l"; }
	elsif ($self->{value} =~ /^r[\w]+$/i)	{ $ret="q"; }
	elsif ($self->{value} =~ /^[a-d][hl]$/i){ $ret="b"; }
	elsif ($self->{value} =~ /^[\w]{2}l$/i)	{ $ret="b"; }
	elsif ($self->{value} =~ /^[\w]{2}$/i)	{ $ret="w"; }
	elsif ($self->{value} =~ /^e[a-z]{2}$/i){ $ret="l"; }

	$ret;
    }
    sub out {
    	my $self = shift;
	if ($gas)	{ sprintf "%s%%%s%s",	$self->{asterisk},
						$self->{value},
						$self->{opmask}; }
	else		{ $self->{opmask} =~ s/%(k[0-7])/$1/;
			  $self->{value}.$self->{opmask}; }
    }
}
{ package label;	                                  
    sub re {
	my	($class, $line) = @_;
	my	$self = {};
	my	$ret;

	if ($$line =~ /(^[\.\w]+)\:/) {
	    bless $self,$class;
	    $self->{value} = $1;
	    $ret = $self;
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;

	    $self->{value} =~ s/^\.L/$decor/;
	}
	$ret;
    }
    sub out {
	my $self = shift;

	if ($gas) {
	    my $func = ($globals{$self->{value}} or $self->{value}) . ":";
	    if ($win64	&& $current_function->{name} eq $self->{value}
			&& $current_function->{abi} eq "svr4") {
		$func .= "\n";
		$func .= "	movq	%rdi,8(%rsp)\n";
		$func .= "	movq	%rsi,16(%rsp)\n";
		$func .= "	movq	%rsp,%rax\n";
		$func .= "${decor}SEH_begin_$current_function->{name}:\n";
		my $narg = $current_function->{narg};
		$narg=6 if (!defined($narg));
		$func .= "	movq	%rcx,%rdi\n" if ($narg>0);
		$func .= "	movq	%rdx,%rsi\n" if ($narg>1);
		$func .= "	movq	%r8,%rdx\n"  if ($narg>2);
		$func .= "	movq	%r9,%rcx\n"  if ($narg>3);
		$func .= "	movq	40(%rsp),%r8\n" if ($narg>4);
		$func .= "	movq	48(%rsp),%r9\n" if ($narg>5);
	    }
	    $func;
	} elsif ($self->{value} ne "$current_function->{name}") {
	                                     
	    $self->{value} .= ":" if ($masm);
	    $self->{value} . ":";
	} elsif ($win64 && $current_function->{abi} eq "svr4") {
	    my $func =	"$current_function->{name}" .
			($nasm ? ":" : "\tPROC $current_function->{scope}") .
			"\n";
	    $func .= "	mov	QWORD$PTR\[8+rsp\],rdi\t;WIN64 prologue\n";
	    $func .= "	mov	QWORD$PTR\[16+rsp\],rsi\n";
	    $func .= "	mov	rax,rsp\n";
	    $func .= "${decor}SEH_begin_$current_function->{name}:";
	    $func .= ":" if ($masm);
	    $func .= "\n";
	    my $narg = $current_function->{narg};
	    $narg=6 if (!defined($narg));
	    $func .= "	mov	rdi,rcx\n" if ($narg>0);
	    $func .= "	mov	rsi,rdx\n" if ($narg>1);
	    $func .= "	mov	rdx,r8\n"  if ($narg>2);
	    $func .= "	mov	rcx,r9\n"  if ($narg>3);
	    $func .= "	mov	r8,QWORD$PTR\[40+rsp\]\n" if ($narg>4);
	    $func .= "	mov	r9,QWORD$PTR\[48+rsp\]\n" if ($narg>5);
	    $func .= "\n";
	} else {
	   "$current_function->{name}".
			($nasm ? ":" : "\tPROC $current_function->{scope}");
	}
    }
}
{ package expr;		                     
    sub re {
	my	($class, $line, $opcode) = @_;
	my	$self = {};
	my	$ret;

	if ($$line =~ /(^[^,]+)/) {
	    bless $self,$class;
	    $self->{value} = $1;
	    $ret = $self;
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;

	    $self->{value} =~ s/\@PLT// if (!$elf);
	    $self->{value} =~ s/([_a-z][_a-z0-9]*)/$globals{$1} or $1/gei;
	    $self->{value} =~ s/\.L/$decor/g;
	    $self->{opcode} = $opcode;
	}
	$ret;
    }
    sub out {
	my $self = shift;
	if ($nasm && $self->{opcode}->mnemonic()=~m/^j(?![re]cxz)/) {
	    "NEAR ".$self->{value};
	} else {
	    $self->{value};
	}
    }
}
{ package cfi_directive;
                                                                   
                                                                   
                                                                   
                                                                   
                                                                  
                
     
                                                                  
                                                            
                    
                                                                
                                                            
                     
                                                            
                                                                 
                    
     
                                                                    
                                                                     
                                                                 
                                                                      
                                                                  
                                                                       
                                                                     
                                   
     
                                                
     
                                                                     
                                                                  
                                                                      
                                                                       

                                                                       
                                                                       
    my %DW_OP_simple = (	                                   
	deref	=> 0x06,	dup	=> 0x12,
	drop	=> 0x13,	over	=> 0x14,
	pick	=> 0x15,	swap	=> 0x16,
	rot	=> 0x17,	xderef	=> 0x18,

	abs	=> 0x19,	and	=> 0x1a,
	div	=> 0x1b,	minus	=> 0x1c,
	mod	=> 0x1d,	mul	=> 0x1e,
	neg	=> 0x1f,	not	=> 0x20,
	or	=> 0x21,	plus	=> 0x22,
	shl	=> 0x24,	shr	=> 0x25,
	shra	=> 0x26,	xor	=> 0x27,
	);

    my %DW_OP_complex = (	                              
	constu		=> 0x10,	         
	consts		=> 0x11,	         
	plus_uconst	=> 0x23,	         
	lit0 		=> 0x30,	                    
	reg0		=> 0x50,	                    
	breg0		=> 0x70,	                             
	regx		=> 0x90,	        
	fbreg		=> 0x91,	         
	bregx		=> 0x92,	                  
	piece		=> 0x93,	         
	);

                                                                   
                                                                        
                                               
    my %DW_reg_idx = (
	"%rax"=>0,  "%rdx"=>1,  "%rcx"=>2,  "%rbx"=>3,
	"%rsi"=>4,  "%rdi"=>5,  "%rbp"=>6,  "%rsp"=>7,
	"%r8" =>8,  "%r9" =>9,  "%r10"=>10, "%r11"=>11,
	"%r12"=>12, "%r13"=>13, "%r14"=>14, "%r15"=>15
	);

    my ($cfa_reg, $cfa_rsp);
    my @cfa_stack;

                                                                      
                                                                    
                                                                      
                                                                     
    sub sleb128 {
	use integer;	                             

	my $val = shift;
	my $sign = ($val < 0) ? -1 : 0;
	my @ret = ();

	while(1) {
	    push @ret, $val&0x7f;

	                                                      
	                                                       
	                   
	    last if (($val>>6) == $sign);

	    @ret[-1] |= 0x80;
	    $val >>= 7;
	}

	return @ret;
    }
    sub uleb128 {
	my $val = shift;
	my @ret = ();

	while(1) {
	    push @ret, $val&0x7f;

	                                           
	    last if (($val >>= 7) == 0);

	    @ret[-1] |= 0x80;
	}

	return @ret;
    }
    sub const {
	my $val = shift;

	if ($val >= 0 && $val < 32) {
            return ($DW_OP_complex{lit0}+$val);
	}
	return ($DW_OP_complex{consts}, sleb128($val));
    }
    sub reg {
	my $val = shift;

	return if ($val !~ m/^(%r\w+)(?:([\+\-])((?:0x)?[0-9a-f]+))?/);

	my $reg = $DW_reg_idx{$1};
	my $off = eval ("0 $2 $3");

	return (($DW_OP_complex{breg0} + $reg), sleb128($off));
	                                                          
	                                                            
	                                                             
	                                    
    }
    sub cfa_expression {
	my $line = shift;
	my @ret;

	foreach my $token (split(/,\s*/,$line)) {
	    if ($token =~ /^%r/) {
		push @ret,reg($token);
	    } elsif ($token =~ /((?:0x)?[0-9a-f]+)\((%r\w+)\)/) {
		push @ret,reg("$2+$1");
	    } elsif ($token =~ /(\w+):(\-?(?:0x)?[0-9a-f]+)(U?)/i) {
		my $i = 1*eval($2);
		push @ret,$DW_OP_complex{$1}, ($3 ? uleb128($i) : sleb128($i));
	    } elsif (my $i = 1*eval($token) or $token eq "0") {
		if ($token =~ /^\+/) {
		    push @ret,$DW_OP_complex{plus_uconst},uleb128($i);
		} else {
		    push @ret,const($i);
		}
	    } else {
		push @ret,$DW_OP_simple{$token};
	    }
	}

	                                                              
	                                                               
	return (15,scalar(@ret),@ret);
    }
    sub re {
	my	($class, $line) = @_;
	my	$self = {};
	my	$ret;

	if ($$line =~ s/^\s*\.cfi_(\w+)\s*//) {
	    bless $self,$class;
	    $ret = $self;
	    undef $self->{value};
	    my $dir = $1;

	    SWITCH: for ($dir) {
	                                                                
	                                                              
	                                                               
	              
	    /startproc/	&& do {	($cfa_reg, $cfa_rsp) = ("%rsp", -8); last; };
	    /endproc/	&& do {	($cfa_reg, $cfa_rsp) = ("%rsp",  0); last; };
	    /def_cfa_register/
			&& do {	$cfa_reg = $$line; last; };
	    /def_cfa_offset/
			&& do {	$cfa_rsp = -1*eval($$line) if ($cfa_reg eq "%rsp");
				last;
			      };
	    /adjust_cfa_offset/
			&& do {	$cfa_rsp -= 1*eval($$line) if ($cfa_reg eq "%rsp");
				last;
			      };
	    /def_cfa/	&& do {	if ($$line =~ /(%r\w+)\s*,\s*(.+)/) {
				    $cfa_reg = $1;
				    $cfa_rsp = -1*eval($2) if ($cfa_reg eq "%rsp");
				}
				last;
			      };
	    /push/	&& do {	$dir = undef;
				$cfa_rsp -= 8;
				if ($cfa_reg eq "%rsp") {
				    $self->{value} = ".cfi_adjust_cfa_offset\t8\n";
				}
				$self->{value} .= ".cfi_offset\t$$line,$cfa_rsp";
				last;
			      };
	    /pop/	&& do {	$dir = undef;
				$cfa_rsp += 8;
				if ($cfa_reg eq "%rsp") {
				    $self->{value} = ".cfi_adjust_cfa_offset\t-8\n";
				}
				$self->{value} .= ".cfi_restore\t$$line";
				last;
			      };
	    /cfa_expression/
			&& do {	$dir = undef;
				$self->{value} = ".cfi_escape\t" .
					join(",", map(sprintf("0x%02x", $_),
						      cfa_expression($$line)));
				last;
			      };
	    /remember_state/
			&& do {	push @cfa_stack, [$cfa_reg, $cfa_rsp];
				last;
			      };
	    /restore_state/
			&& do {	($cfa_reg, $cfa_rsp) = @{pop @cfa_stack};
				last;
			      };
	    }

	    $self->{value} = ".cfi_$dir\t$$line" if ($dir);

	    $$line = "";
	}

	return $ret;
    }
    sub out {
	my $self = shift;
	return ($elf ? $self->{value} : undef);
    }
}
{ package seh_directive;
                                                                         
                                          
                                                                                      
                                                                                
                                                                          
                                                                     
     
                                                                             
                                                                               
                                 
     
                                                                                
                                                                             
                                                                              
                                                                           
                                                                          
                                                                        
     
                                                                       
                                                                             
                                                                              
                                                                            
                                                                              
                                                                                
                                                                        
                                                                      

    my $UWOP_PUSH_NONVOL = 0;
    my $UWOP_ALLOC_LARGE = 1;
    my $UWOP_ALLOC_SMALL = 2;
    my $UWOP_SET_FPREG = 3;
    my $UWOP_SAVE_NONVOL = 4;
    my $UWOP_SAVE_NONVOL_FAR = 5;
    my $UWOP_SAVE_XMM128 = 8;
    my $UWOP_SAVE_XMM128_FAR = 9;

    my %UWOP_REG_TO_NUMBER = ("%rax" => 0, "%rcx" => 1, "%rdx" => 2, "%rbx" => 3,
			      "%rsp" => 4, "%rbp" => 5, "%rsi" => 6, "%rdi" => 7,
			      map(("%r$_" => $_), (8..15)));
    my %UWOP_NUMBER_TO_REG = reverse %UWOP_REG_TO_NUMBER;

                                                          
    my ($xdata, $pdata) = ("", "");

    my %info;

    my $next_label = 0;
    my $current_label_func = "";

                                                                  
    sub _new_unwind_label {
	my ($name) = (@_);
	                                                                        
	                                      
	my $func = $current_function->{name};
	if ($func ne $current_label_func) {
	    $current_label_func = $func;
	    $next_label = 0;
	}

	my $num = $next_label++;
	return ".LSEH_${name}_${func}_${num}";
    }

    sub _check_in_proc {
	die "Missing .seh_startproc directive" unless %info;
    }

    sub _check_in_prologue {
	_check_in_proc();
	die "Invalid SEH directive after .seh_endprologue" if defined($info{endprologue});
    }

    sub _check_not_in_proc {
	die "Missing .seh_endproc directive" if %info;
    }

    sub _startproc {
	_check_not_in_proc();
	if ($current_function->{abi} eq "svr4") {
	    die "SEH directives can only be used with \@abi-omnipotent";
	}

	my $info_label = _new_unwind_label("info");
	my $start_label = _new_unwind_label("begin");
	%info = (
	                                                                
	    info_label => $info_label,
	                                               
	    start_label => $start_label,
	                                                          
	    endprologue => undef,
	                                                             
	                                          
	    unwind_codes => "",
	                                                              
	    num_codes => 0,
	                                                               
	                    
	    frame_reg => 0,
	                                                                      
	                                     
	    frame_offset => 0,
	                                                            
	                                                           
	                                                                     
	    has_offset => 0,
	                                                     
	                                                             
	                                        
	    has_nonpushreg => 0,
	);
	return $start_label;
    }

    sub _add_unwind_code {
	my ($op, $value, @extra) = @_;
	_check_in_prologue();
	if ($op != $UWOP_PUSH_NONVOL) {
	    $info{has_nonpushreg} = 1;
	} elsif ($info{has_nonpushreg}) {
	    die ".seh_pushreg directives must appear first in the prologue";
	}

	my $label = _new_unwind_label("prologue");
	                                      
	                                                                                                     
	my $encoded = $op | ($value << 4);
	my $codes = <<____;
	.byte	$label-$info{start_label}
	.byte	$encoded
____
	                                                           
	foreach (@extra) {
	    $codes .= "\t.value\t$_\n";
	}

	$info{num_codes} += 1 + scalar(@extra);
	                                           
	$info{unwind_codes} = $codes . $info{unwind_codes};
	return $label;
    }

    sub _updating_fixed_allocation {
	_check_in_prologue();
	if ($info{frame_reg} != 0) {
	                                                                      
	                                                                        
	                                                                        
	                  
	    die "fixed allocation may not be increased after .seh_setframe";
	}
	if ($info{has_offset}) {
	                                                                   
	                                                                     
	                                                                      
	                       
	    die "directives with an offset must come after the fixed allocation is established.";
	}
    }

    sub _endproc {
	_check_in_proc();
	if (!defined($info{endprologue})) {
	    die "Missing .seh_endprologue";
	}

	my $end_label = _new_unwind_label("end");
	                                
	                                                                                                          
	$pdata .= <<____;
	.rva	$info{start_label}
	.rva	$end_label
	.rva	$info{info_label}

____

	                            
	                                                                                                     
	my $frame_encoded = $info{frame_reg} | (($info{frame_offset} / 16) << 4);
	$xdata .= <<____;
$info{info_label}:
	.byte	1	# version 1, no flags
	.byte	$info{endprologue}-$info{start_label}
	.byte	$info{num_codes}
	.byte	$frame_encoded
$info{unwind_codes}
____

	                                                                      
	                                                                  
	                                                                      
	                                                                       
	                                                                   
	                      
	if ($info{num_codes} & 1) {
	    $xdata .= "\t.value\t0\n";
	}

	%info = ();
	return $end_label;
    }

    sub re {
	my ($class, $line) = @_;
	if ($$line =~ s/^\s*\.seh_(\w+)\s*//) {
	    my $dir = $1;
	    if (!$win64) {
		$$line = "";
		return;
	    }

	    my $label;
	    SWITCH: for ($dir) {
		/^startproc$/ && do {
		    $label = _startproc($1);
		    last;
		};
		/^pushreg$/ && do {
		    $$line =~ /^(%\w+)\s*$/ or die "could not parse .seh_$dir";
		    my $reg_num = $UWOP_REG_TO_NUMBER{$1} or die "unknown register $1";
		    _updating_fixed_allocation();
		    $label = _add_unwind_code($UWOP_PUSH_NONVOL, $reg_num);
		    last;
		};
		/^stackalloc$/ && do {
		    my $num = eval($$line);
		    if ($num <= 0 || $num % 8 != 0) {
			die "invalid stack allocation: $num";
		    }
		    _updating_fixed_allocation();
		    if ($num <= 128) {
			$label = _add_unwind_code($UWOP_ALLOC_SMALL, ($num - 8) / 8);
		    } elsif ($num < 512 * 1024) {
			$label = _add_unwind_code($UWOP_ALLOC_LARGE, 0, $num / 8);
		    } elsif ($num < 4 * 1024 * 1024 * 1024) {
			$label = _add_unwind_code($UWOP_ALLOC_LARGE, 1, $num >> 16, $num & 0xffff);
		    } else {
			die "stack allocation too large: $num"
		    }
		    last;
		};
		/^setframe$/ && do {
		    if ($info{frame_reg} != 0) {
			die "duplicate .seh_setframe directive";
		    }
		    if ($info{has_offset}) {
			die "directives with with an offset must come after .seh_setframe.";
		    }
		    $$line =~ /(%\w+)\s*,\s*(.+)/ or die "could not parse .seh_$dir";
		    my $reg_num = $UWOP_REG_TO_NUMBER{$1} or die "unknown register $1";
		    my $offset = eval($2);
		    if ($offset < 0 || $offset % 16 != 0 || $offset > 240) {
			die "invalid offset: $offset";
		    }
		    $info{frame_reg} = $reg_num;
		    $info{frame_offset} = $offset;
		    $label = _add_unwind_code($UWOP_SET_FPREG, 0);
		    last;
		};
		/^savereg$/ && do {
		    $$line =~ /(%\w+)\s*,\s*(.+)/ or die "could not parse .seh_$dir";
		    my $reg_num = $UWOP_REG_TO_NUMBER{$1} or die "unknown register $1";
		    my $offset = eval($2);
		    if ($offset < 0 || $offset % 8 != 0) {
			die "invalid offset: $offset";
		    }
		    if ($offset < 8 * 65536) {
			$label = _add_unwind_code($UWOP_SAVE_NONVOL, $reg_num, $offset / 8);
		    } else {
			$label = _add_unwind_code($UWOP_SAVE_NONVOL_FAR, $reg_num, $offset >> 16, $offset & 0xffff);
		    }
		    $info{has_offset} = 1;
		    last;
		};
		/^savexmm$/ && do {
		    $$line =~ /%xmm(\d+)\s*,\s*(.+)/ or die "could not parse .seh_$dir";
		    my $reg_num = $1;
		    my $offset = eval($2);
		    if ($offset < 0 || $offset % 16 != 0) {
			die "invalid offset: $offset";
		    }
		    if ($offset < 16 * 65536) {
			$label = _add_unwind_code($UWOP_SAVE_XMM128, $reg_num, $offset / 16);
		    } else {
			$label = _add_unwind_code($UWOP_SAVE_XMM128_FAR, $reg_num, $offset >> 16, $offset & 0xffff);
		    }
		    $info{has_offset} = 1;
		    last;
		};
		/^endprologue$/ && do {
		    _check_in_prologue();
		    if ($info{num_codes} == 0) {
			                                                  
			                                                        
			                                             
			die ".seh_endprologue found with no unwind codes";
		    }

		    $label = _new_unwind_label("endprologue");
		    $info{endprologue} = $label;
		    last;
		};
		/^endproc$/ && do {
		    $label = _endproc();
		    last;
		};
		die "unknown SEH directive .seh_$dir";
	    }

	                                                                    
	                    
	    $$line = "";
	    $label .= ":";
	    return label->re(\$label);
	}
    }

    sub pdata_and_xdata {
	return "" unless $win64;

	my $ret = "";
	if ($pdata ne "") {
	    $ret .= <<____;
.section	.pdata
.align	4
$pdata
____
	}
	if ($xdata ne "") {
	    $ret .= <<____;
.section	.xdata
.align	4
$xdata
____
	}
	return $ret;
    }
}
{ package directive;	                                        
    my %sections;
    sub nasm_section {
	my ($name, $qualifiers) = @_;
	my $ret = "section\t$name";
	if (exists $sections{$name}) {
	                                                                        
	                                                                     
	                                                                     
	                
	     
	                                                         
	    my $old = $sections{$name};
	    die "Inconsistent qualifiers: $qualifiers vs $old" if ($qualifiers ne "" && $qualifiers ne $old);
	} else {
	    $sections{$name} = $qualifiers;
	    if ($qualifiers ne "") {
		$ret .= " $qualifiers";
	    }
	}
	return $ret;
    }
    sub re {
	my	($class, $line) = @_;
	my	$self = {};
	my	$ret;
	my	$dir;

	                                                
	$ret = cfi_directive->re($line) and return $ret;
	$ret = seh_directive->re($line) and return $ret;

	if ($$line =~ /^\s*(\.\w+)/) {
	    bless $self,$class;
	    $dir = $1;
	    $ret = $self;
	    undef $self->{value};
	    $$line = substr($$line,@+[0]); $$line =~ s/^\s+//;

	    SWITCH: for ($dir) {
		/\.global|\.globl|\.extern/
			    && do { $globals{$$line} = $prefix . $$line;
				    $$line = $globals{$$line} if ($prefix);
				    last;
				  };
		/\.type/    && do { my ($sym,$type,$narg) = split(/\s*,\s*/,$$line);
				    if ($type eq "\@function") {
					undef $current_function;
					$current_function->{name} = $sym;
					$current_function->{abi}  = "svr4";
					$current_function->{narg} = $narg;
					$current_function->{scope} = defined($globals{$sym})?"PUBLIC":"PRIVATE";
				    } elsif ($type eq "\@abi-omnipotent") {
					undef $current_function;
					$current_function->{name} = $sym;
					$current_function->{scope} = defined($globals{$sym})?"PUBLIC":"PRIVATE";
				    }
				    $$line =~ s/\@abi\-omnipotent/\@function/;
				    $$line =~ s/\@function.*/\@function/;
				    last;
				  };
		/\.asciz/   && do { if ($$line =~ /^"(.*)"$/) {
					$dir  = ".byte";
					$$line = join(",",unpack("C*",$1),0);
				    }
				    last;
				  };
		/\.rva|\.long|\.quad|\.byte/
			    && do { $$line =~ s/([_a-z][_a-z0-9]*)/$globals{$1} or $1/gei;
				    $$line =~ s/\.L/$decor/g;
				    last;
				  };
	    }

	    if ($gas) {
		$self->{value} = $dir . "\t" . $$line;

		if ($dir =~ /\.extern/) {
		    if ($flavour eq "elf") {
			$self->{value} .= "\n.hidden $$line";
		    } else {
			$self->{value} = "";
		    }
		} elsif (!$elf && $dir =~ /\.type/) {
		    $self->{value} = "";
		    $self->{value} = ".def\t" . ($globals{$1} or $1) . ";\t" .
				(defined($globals{$1})?".scl 2;":".scl 3;") .
				"\t.type 32;\t.endef"
				if ($win64 && $$line =~ /([^,]+),\@function/);
		} elsif (!$elf && $dir =~ /\.size/) {
		    $self->{value} = "";
		    if (defined($current_function)) {
			$self->{value} .= "${decor}SEH_end_$current_function->{name}:"
				if ($win64 && $current_function->{abi} eq "svr4");
			undef $current_function;
		    }
		} elsif (!$elf && $dir =~ /\.align/) {
		    $self->{value} = ".p2align\t" . (log($$line)/log(2));
		} elsif ($dir eq ".section") {
		    $current_segment=$$line;
		    if (!$elf && $current_segment eq ".rodata") {
			if	($flavour eq "macosx") { $self->{value} = ".section\t__DATA,__const"; }
		    }
		    if (!$elf && $current_segment eq ".init") {
			if	($flavour eq "macosx")	{ $self->{value} = ".mod_init_func"; }
			elsif	($flavour eq "mingw64")	{ $self->{value} = ".section\t.ctors"; }
		    }
		} elsif ($dir =~ /\.(text|data)/) {
		    $current_segment=".$1";
		} elsif ($dir =~ /\.global|\.globl|\.extern/) {
		    if ($flavour eq "macosx") {
		        $self->{value} .= "\n.private_extern $$line";
		    } else {
		        $self->{value} .= "\n.hidden $$line";
		    }
		} elsif ($dir =~ /\.hidden/) {
		    if    ($flavour eq "macosx")  { $self->{value} = ".private_extern\t$prefix$$line"; }
		    elsif ($flavour eq "mingw64") { $self->{value} = ""; }
		} elsif ($dir =~ /\.comm/) {
		    $self->{value} = "$dir\t$prefix$$line";
		    $self->{value} =~ s|,([0-9]+),([0-9]+)$|",$1,".log($2)/log(2)|e if ($flavour eq "macosx");
		}
		$$line = "";
		return $self;
	    }

	                               
	    SWITCH: for ($dir) {
		/\.text/    && do { my $v=undef;
				    if ($nasm) {
					$v=nasm_section(".text", "code align=64")."\n";
				    } else {
					$v="$current_segment\tENDS\n" if ($current_segment);
					$current_segment = ".text\$";
					$v.="$current_segment\tSEGMENT ";
					$v.=$masm>=$masmref ? "ALIGN(256)" : "PAGE";
					$v.=" 'CODE'";
				    }
				    $self->{value} = $v;
				    last;
				  };
		/\.data/    && do { my $v=undef;
				    if ($nasm) {
					$v=nasm_section(".data", "data align=8")."\n";
				    } else {
					$v="$current_segment\tENDS\n" if ($current_segment);
					$current_segment = "_DATA";
					$v.="$current_segment\tSEGMENT";
				    }
				    $self->{value} = $v;
				    last;
				  };
		/\.section/ && do { my $v=undef;
				    $$line =~ s/([^,]*).*/$1/;
				    $$line = ".CRT\$XCU" if ($$line eq ".init");
				    $$line = ".rdata" if ($$line eq ".rodata");
				    if ($nasm) {
					my $qualifiers = "";
					if ($$line=~/\.([prx])data/) {
					    $qualifiers = "rdata align=";
					    $qualifiers .= $1 eq "p"? 4 : 8;
					} elsif ($$line=~/\.CRT\$/i) {
					    $qualifiers = "rdata align=8";
					}
					$v = nasm_section($$line, $qualifiers);
				    } else {
					$v="$current_segment\tENDS\n" if ($current_segment);
					$v.="$$line\tSEGMENT";
					if ($$line=~/\.([prx])data/) {
					    $v.=" READONLY";
					    $v.=" ALIGN(".($1 eq "p" ? 4 : 8).")" if ($masm>=$masmref);
					} elsif ($$line=~/\.CRT\$/i) {
					    $v.=" READONLY ";
					    $v.=$masm>=$masmref ? "ALIGN(8)" : "DWORD";
					}
				    }
				    $current_segment = $$line;
				    $self->{value} = $v;
				    last;
				  };
		/\.extern/  && do { $self->{value}  = "EXTERN\t".$$line;
				    $self->{value} .= ":NEAR" if ($masm);
				    last;
				  };
		/\.globl|.global/
			    && do { $self->{value}  = $masm?"PUBLIC":"global";
				    $self->{value} .= "\t".$$line;
				    last;
				  };
		/\.size/    && do { if (defined($current_function)) {
					undef $self->{value};
					if ($current_function->{abi} eq "svr4") {
					    $self->{value}="${decor}SEH_end_$current_function->{name}:";
					    $self->{value}.=":\n" if($masm);
					}
					$self->{value}.="$current_function->{name}\tENDP" if($masm && $current_function->{name});
					undef $current_function;
				    }
				    last;
				  };
		/\.align/   && do { my $max = ($masm && $masm>=$masmref) ? 256 : 4096;
				    $self->{value} = "ALIGN\t".($$line>$max?$max:$$line);
				    last;
				  };
		/\.(value|long|rva|quad)/
			    && do { my $sz  = substr($1,0,1);
				    my @arr = split(/,\s*/,$$line);
				    my $last = pop(@arr);
				    my $conv = sub  {	my $var=shift;
							$var=~s/^(0b[0-1]+)/oct($1)/eig;
							$var=~s/^0x([0-9a-f]+)/0$1h/ig if ($masm);
							if ($sz eq "D" && ($current_segment=~/.[px]data/ || $dir eq ".rva"))
							{ $var=~s/^([_a-z\$\@][_a-z0-9\$\@]*)/$nasm?"$1 wrt ..imagebase":"imagerel $1"/egi; }
							$var;
						    };

				    $sz =~ tr/bvlrq/BWDDQ/;
				    $self->{value} = "\tD$sz\t";
				    for (@arr) { $self->{value} .= &$conv($_).","; }
				    $self->{value} .= &$conv($last);
				    last;
				  };
		/\.byte/    && do { my @str=split(/,\s*/,$$line);
				    map(s/(0b[0-1]+)/oct($1)/eig,@str);
				    map(s/0x([0-9a-f]+)/0$1h/ig,@str) if ($masm);
				    while ($#str>15) {
					$self->{value}.="\tDB\t"
						.join(",",@str[0..15])."\n";
					foreach (0..15) { shift @str; }
				    }
				    $self->{value}.="\tDB\t"
						.join(",",@str) if (@str);
				    last;
				  };
		/\.comm/    && do { my @str=split(/,\s*/,$$line);
				    my $v=undef;
				    if ($nasm) {
					$v.="common	$prefix@str[0] @str[1]";
				    } else {
					$v="$current_segment\tENDS\n" if ($current_segment);
					$current_segment = "_DATA";
					$v.="$current_segment\tSEGMENT\n";
					$v.="COMM	@str[0]:DWORD:".@str[1]/4;
				    }
				    $self->{value} = $v;
				    last;
				  };
	    }
	    $$line = "";
	}

	$ret;
    }
    sub out {
	my $self = shift;
	$self->{value};
    }
}

                                                                        

{
  my $comment = "//";
  $comment = ";" if ($masm || $nasm);
  print <<___;
$comment This file is generated from a similarly-named Perl script in the BoringSSL
$comment source tree. Do not edit by hand.

___
}

if ($nasm) {
    die "unknown target" unless ($win64);
    print <<___;
\%ifidn __OUTPUT_FORMAT__, win64
default	rel
\%define XMMWORD
\%define YMMWORD
\%define ZMMWORD
\%define _CET_ENDBR

\%ifdef BORINGSSL_PREFIX
\%include "boringssl_prefix_symbols_nasm.inc"
\%endif
___
} elsif ($masm) {
    print <<___;
OPTION	DOTNAME
___
}

if ($gas) {
    my $target;
    if ($elf) {
                                                                              
                            
        $target = "defined(__ELF__)";
    } elsif ($apple) {
        $target = "defined(__APPLE__)";
    } else {
        die "unknown target: $flavour";
    }
    print <<___;
#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && $target
___
}

sub process_line {
    my $line = shift;
    $line =~ s|\R$||;                         

    if ($nasm) {
	$line =~ s|^#ifdef |%ifdef |;
	$line =~ s|^#ifndef |%ifndef |;
	$line =~ s|^#endif|%endif|;
	$line =~ s|[#!].*$||;	                                  
    } else {
	                                                                    
	                                                                        
	                   
	$line =~ s|!.*$||;
	$line =~ s|(?<=.)#.*$||;
	$line =~ s|^#([^a-z].*)?$||;
    }

    $line =~ s|/\*.*\*/||;	                             
    $line =~ s|^\s+||;		                                        
    $line =~ s|\s+$||;		                    

    if (my $label=label->re(\$line))	{ print $label->out(); }

    if (my $directive=directive->re(\$line)) {
	printf "%s",$directive->out();
    } elsif (my $opcode=opcode->re(\$line)) {
	my $asm = eval("\$".$opcode->mnemonic());

	if ((ref($asm) eq 'CODE') && scalar(my @bytes=&$asm($line))) {
	    print $gas?".byte\t":"DB\t",join(',',@bytes),"\n";
	    next;
	}

	my @args;
	ARGUMENT: while (1) {
	    my $arg;

	    ($arg=register->re(\$line, $opcode))||
	    ($arg=const->re(\$line))		||
	    ($arg=ea->re(\$line, $opcode))	||
	    ($arg=expr->re(\$line, $opcode))	||
	    last ARGUMENT;

	    push @args,$arg;

	    last ARGUMENT if ($line !~ /^,/);

	    $line =~ s/^,\s*//;
	}            

	if ($#args>=0) {
	    my $insn;
	    my $sz=$opcode->size();

	    if ($gas) {
		$insn = $opcode->out($#args>=1?$args[$#args]->size():$sz);
		@args = map($_->out($sz),@args);
		printf "\t%s\t%s",$insn,join(",",@args);
	    } else {
		$insn = $opcode->out();
		foreach (@args) {
		    my $arg = $_->out();
		                                                  
		    if ($arg =~ /^xmm[0-9]+$/) { $insn.=$sz; $sz="x" if(!$sz); last; }
		    if ($arg =~ /^ymm[0-9]+$/) { $insn.=$sz; $sz="y" if(!$sz); last; }
		    if ($arg =~ /^zmm[0-9]+$/) { $insn.=$sz; $sz="z" if(!$sz); last; }
		    if ($arg =~ /^mm[0-9]+$/)  { $insn.=$sz; $sz="q" if(!$sz); last; }
		}
		@args = reverse(@args);
		undef $sz if ($nasm && $opcode->mnemonic() eq "lea");
		printf "\t%s\t%s",$insn,join(",",map($_->out($sz),@args));
	    }
	} else {
	    printf "\t%s",$opcode->out();
	}
    }

    print $line,"\n";
}

while(defined(my $line=<>)) {
    process_line($line);
}
foreach my $line (split(/\n/, seh_directive->pdata_and_xdata())) {
    process_line($line);
}

print "\n$current_segment\tENDS\n"	if ($current_segment && $masm);
if ($masm) {
    print "END\n";
} elsif ($gas) {
    print "#endif\n";
} elsif ($nasm) {
    print <<___;
\%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
\%endif
___
} else {
    die "unknown assembler";
}

close STDOUT or die "error closing STDOUT: $!";

                                                 
                                   
 
               
            
            
              
              
             
             
            
            
             
             
            
            
            
            
            
            
 
                       
                         
                            
 
                                                                    
                                                                     
                                                                  
                                                                   
                                                                
                     
 
                                                                     
                                                                   
                                                                    
                                                                     
                                                              
                                                                   
                                                                      
                                                                    
                                                                  
 
                                                                  
                                                                     
                                                                     
                                                                     
 
                      
             
                   
                    
                                                      
                                                  
                                        
                                     
                                
                                
       
     
             
                   
                    
       
     
 
                                                 
                                           
 
                                                                     
                                                                     
                                                                 
                                                                
                                                                     
                                                                     
                                                                 
                                                                
                                                              
                                                                   
                                                                      
                                                                       
          
 
                          
           
                                                
                                           
            
            
                
                                                 
                
                                          
                                                  
                                             
              
     
                                             
                                                      
                     
                    
                                       
                 
     
                           
 
                                                                      
                                                                      
                                                                    
                                                                     
                                         
 
                                                                     
                                             
                                           
                              
 
                         
                                    
                               
                                
                          
                          
                          
       
   
                              
                        
                        
 
                                                       
                                                     
                                                           
                                                    
                                 
   
 
                                                                      
                                                                 
                                                                        
                        
 
                    
                    
                    
                    
                    
                    
                    
                    
                   
                   
                    
                    
                    
                    
                    
                    
                    
                     
                         
                                 
                                 
                                     
                                        
                                 
                                     
                                       
                                    
                       
                             
 
                                                                
                                                                  
 
                                                   
 
                       
               
              
 
                                                                 
                                                         
 
                                                                     
                                                                    
                                                                     
                                                                    
                                                                     
                                                                        
                                                                     
                                                   
 
                           
                         
                           
 
                                                                      
                                                                   
                                                                   
                                                                     
                                                                     
                                                                      
          
 
                                                                     
                                                                     
                                                                  
                                                                     
                                                                       
                                                                   
                                                               
                                                                   
                                                                       
                                                                    
 
                                                                     
                                                                     
                                                                     
                                                                     
                                                                       
                                               
 
                                                                     
                                                             
                                                               
                                                               
                                             
