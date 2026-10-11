#! /usr/bin/env perl
# Copyright 1998-2016 The OpenSSL Project Authors. All Rights Reserved.
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


# ====================================================================
# [Re]written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project.
# ====================================================================

# "[Re]written" was achieved in two major overhauls. In 2004 BODY_*
# functions were re-implemented to address P4 performance issue [see
# commentary below], and in 2006 the rest was rewritten in order to
# gain freedom to liberate licensing terms.

# January, September 2004.
#
# It was noted that Intel IA-32 C compiler generates code which
# performs ~30% *faster* on P4 CPU than original *hand-coded*
# SHA1 assembler implementation. To address this problem (and
# prove that humans are still better than machines:-), the
# original code was overhauled, which resulted in following
# performance changes:
#
#		compared with original	compared with Intel cc
#		assembler impl.		generated code
# Pentium	-16%			+48%
# PIII/AMD	+8%			+16%
# P4		+85%(!)			+45%
#
# As you can see Pentium came out as looser:-( Yet I reckoned that
# improvement on P4 outweighs the loss and incorporate this
# re-tuned code to 0.9.7 and later.
# ----------------------------------------------------------------

# August 2009.
#
# George Spelvin has tipped that F_40_59(b,c,d) can be rewritten as
# '(c&d) + (b&(c^d))', which allows to accumulate partial results
# and lighten "pressure" on scratch registers. This resulted in
# >12% performance improvement on contemporary AMD cores (with no
# degradation on other CPUs:-). Also, the code was revised to maximize
# "distance" between instructions producing input to 'lea' instruction
# and the 'lea' instruction itself, which is essential for Intel Atom
# core and resulted in ~15% improvement.

# October 2010.
#
# Add SSSE3, Supplemental[!] SSE3, implementation. The idea behind it
# is to offload message schedule denoted by Wt in NIST specification,
# or Xupdate in OpenSSL source, to SIMD unit. The idea is not novel,
# and in SSE2 context was first explored by Dean Gaudet in 2004, see
# http://arctic.org/~dean/crypto/sha1.html. Since then several things
# have changed that made it interesting again:
#
# a) XMM units became faster and wider;
# b) instruction set became more versatile;
# c) an important observation was made by Max Locktykhin, which made
#    it possible to reduce amount of instructions required to perform
#    the operation in question, for further details see
#    http://software.intel.com/en-us/articles/improving-the-performance-of-the-secure-hash-algorithm-1/.

# April 2011.
#
# Add AVX code path, probably most controversial... The thing is that
# switch to AVX alone improves performance by as little as 4% in
# comparison to SSSE3 code path. But below result doesn't look like
# 4% improvement... Trouble is that Sandy Bridge decodes 'ro[rl]' as
# pair of µ-ops, and it's the additional µ-ops, two per round, that
# make it run slower than Core2 and Westmere. But 'sh[rl]d' is decoded
# as single µ-op by Sandy Bridge and it's replacing 'ro[rl]' with
# equivalent 'sh[rl]d' that is responsible for the impressive 5.1
# cycles per processed byte. But 'sh[rl]d' is not something that used
# to be fast, nor does it appear to be fast in upcoming Bulldozer
# [according to its optimization manual]. Which is why AVX code path
# is guarded by *both* AVX and synthetic bit denoting Intel CPUs.
# One can argue that it's unfair to AMD, but without 'sh[rl]d' it
# makes no sense to keep the AVX code path. If somebody feels that
# strongly, it's probably more appropriate to discuss possibility of
# using vector rotate XOP on AMD...

# March 2014.
#
# Add support for Intel SHA Extensions.

######################################################################
# Current performance is summarized in following table. Numbers are
# CPU clock cycles spent to process single byte (less is better).
#
#		x86		SSSE3		AVX
# Pentium	15.7		-
# PIII		11.5		-
# P4		10.6		-
# AMD K8	7.1		-
# Core2		7.3		6.0/+22%	-
# Westmere	7.3		5.5/+33%	-
# Sandy Bridge	8.8		6.2/+40%	5.1(**)/+73%
# Ivy Bridge	7.2		4.8/+51%	4.7(**)/+53%
# Haswell	6.5		4.3/+51%	4.1(**)/+58%
# Skylake	6.4		4.1/+55%	4.1(**)/+55%
# Bulldozer	11.6		6.0/+92%
# VIA Nano	10.6		7.5/+41%
# Atom		12.5		9.3(*)/+35%
# Silvermont	14.5		9.9(*)/+46%
# Goldmont	8.8		6.7/+30%	1.7(***)/+415%
#
# (*)	Loop is 1056 instructions long and expected result is ~8.25.
#	The discrepancy is because of front-end limitations, so
#	called MS-ROM penalties, and on Silvermont even rotate's
#	limited parallelism.
#
# (**)	As per above comment, the result is for AVX *plus* sh[rl]d.
#
# (***)	SHAEXT result

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC,"${dir}","${dir}../../../perlasm");
require "x86asm.pl";

$output=pop;
open STDOUT,">$output";

&asm_init($ARGV[0]);

$xmm = 1;

                                                                          
                                                                           
                                      
$ymm = 1;

$shaext=$xmm;	                                      

                                                                           
              
$shaext = 0;


$A="eax";
$B="ebx";
$C="ecx";
$D="edx";
$E="edi";
$T="esi";
$tmp1="ebp";

@V=($A,$B,$C,$D,$E,$T);

$alt=0;	                                                           
	                                                           
	                 

sub BODY_00_15
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;

	&comment("00_15 $n");

	&mov($f,$c);			                          
	 if ($n==0)  { &mov($tmp1,$a); }
	 else        { &mov($a,$tmp1); }
	&rotl($tmp1,5);			                  
	 &xor($f,$d);
	&add($tmp1,$e);			          
	 &mov($e,&swtmp($n%16));	                                  
	 				                                   
					                    
	&and($f,$b);
	&rotr($b,2);			                
	 &xor($f,$d);			                        
	&lea($tmp1,&DWP(0x5a827999,$tmp1,$e));	                  

	if ($n==15) { &mov($e,&swtmp(($n+1)%16));                            
		      &add($f,$tmp1); }	         
	else        { &add($tmp1,$f); }	                           
	&mov($tmp1,$a)			if ($alt && $n==15);
	}

sub BODY_16_19
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;

	&comment("16_19 $n");

if ($alt) {
	&xor($c,$d);
	 &xor($f,&swtmp(($n+2)%16));	                                   
	&and($tmp1,$c);			                                     
	 &xor($f,&swtmp(($n+8)%16));
	&xor($tmp1,$d);			                     
	 &xor($f,&swtmp(($n+13)%16));	                     
	&rotl($f,1);			               
	 &add($e,$tmp1);		                   
	&xor($c,$d);			            
	 &mov($tmp1,$a);		                 
	&rotr($b,$n==16?2:7);		                
	 &mov(&swtmp($n%16),$f);	      
	&rotl($a,5);			             
	 &lea($f,&DWP(0x5a827999,$f,$e));                     
	&mov($e,&swtmp(($n+1)%16));	                            
	 &add($f,$a);			                
} else {
	&mov($tmp1,$c);			                             
	 &xor($f,&swtmp(($n+2)%16));	                                   
	&xor($tmp1,$d);
	 &xor($f,&swtmp(($n+8)%16));
	&and($tmp1,$b);
	 &xor($f,&swtmp(($n+13)%16));	                     
	&rotl($f,1);			               
	 &xor($tmp1,$d);		                     
	&add($e,$tmp1);			                   
	 &mov($tmp1,$a);
	&rotr($b,2);			                
	 &mov(&swtmp($n%16),$f);	      
	&rotl($tmp1,5);			             
	 &lea($f,&DWP(0x5a827999,$f,$e));                     
	&mov($e,&swtmp(($n+1)%16));	                            
	 &add($f,$tmp1);		                
}
	}

sub BODY_20_39
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;
	local $K=($n<40)?0x6ed9eba1:0xca62c1d6;

	&comment("20_39 $n");

if ($alt) {
	&xor($tmp1,$c);			                                   
	 &xor($f,&swtmp(($n+2)%16));	                                   
	&xor($tmp1,$d);			                           
	 &xor($f,&swtmp(($n+8)%16));
	&add($e,$tmp1);			                   
	 &xor($f,&swtmp(($n+13)%16));	                     
	&rotl($f,1);			               
	 &mov($tmp1,$a);		                 
	&rotr($b,7);			                
	 &mov(&swtmp($n%16),$f)		if($n<77);      
	&rotl($a,5);			             
	 &xor($b,$c)			if($n==39);                        
	&and($tmp1,$b)			if($n==39);
	 &lea($f,&DWP($K,$f,$e));	              
	&mov($e,&swtmp(($n+1)%16))	if($n<79);                            
	 &add($f,$a);			                
	&rotr($a,5)			if ($n==79);
} else {
	&mov($tmp1,$b);			                             
	 &xor($f,&swtmp(($n+2)%16));	                                   
	&xor($tmp1,$c);
	 &xor($f,&swtmp(($n+8)%16));
	&xor($tmp1,$d);			                           
	 &xor($f,&swtmp(($n+13)%16));	                     
	&rotl($f,1);			               
	 &add($e,$tmp1);		                   
	&rotr($b,2);			                
	 &mov($tmp1,$a);
	&rotl($tmp1,5);			             
	 &mov(&swtmp($n%16),$f) if($n<77);      
	&lea($f,&DWP($K,$f,$e));	              
	 &mov($e,&swtmp(($n+1)%16)) if($n<79);                            
	&add($f,$tmp1);			                
}
	}

sub BODY_40_59
	{
	local($n,$a,$b,$c,$d,$e,$f)=@_;

	&comment("40_59 $n");

if ($alt) {
	&add($e,$tmp1);			            
	 &xor($f,&swtmp(($n+2)%16));	                                   
	&mov($tmp1,$d);
	 &xor($f,&swtmp(($n+8)%16));
	&xor($c,$d);			            
	 &xor($f,&swtmp(($n+13)%16));	                     
	&rotl($f,1);			               
	 &and($tmp1,$c);
	&rotr($b,7);			                
	 &add($e,$tmp1);		        
	&mov($tmp1,$a);			                 
	 &mov(&swtmp($n%16),$f);	      
	&rotl($a,5);			             
	 &xor($b,$c)			if ($n<59);
	&and($tmp1,$b)			if ($n<59);                             
	 &lea($f,&DWP(0x8f1bbcdc,$f,$e));                        
	&mov($e,&swtmp(($n+1)%16));	                            
	 &add($f,$a);			                
} else {
	&mov($tmp1,$c);			                             
	 &xor($f,&swtmp(($n+2)%16));	                                   
	&xor($tmp1,$d);
	 &xor($f,&swtmp(($n+8)%16));
	&and($tmp1,$b);
	 &xor($f,&swtmp(($n+13)%16));	                     
	&rotl($f,1);			               
	 &add($tmp1,$e);		            
	&rotr($b,2);			                
	 &mov($e,$a);			                    
	&rotl($e,5);			             
	 &mov(&swtmp($n%16),$f);	      
	&lea($f,&DWP(0x8f1bbcdc,$f,$tmp1));                        
	 &mov($tmp1,$c);
	&add($f,$e);			                
	 &and($tmp1,$d);
	&mov($e,&swtmp(($n+1)%16));	                            
	 &add($f,$tmp1);		        
}
	}

&static_label("K_XX_XX");

&function_begin("sha1_block_data_order_nohw");
	&mov($tmp1,&wparam(0));	            
	&mov($T,&wparam(1));	                   
	&mov($A,&wparam(2));	            
	&stack_push(16+3);	                
	&shl($A,6);
	&add($A,$T);
	&mov(&wparam(2),$A);	                                 
	&mov($E,&DWP(16,$tmp1));            
	&jmp(&label("loop"));

&set_label("loop",16);

	                                                  
	for ($i=0; $i<16; $i+=4)
		{
		&mov($A,&DWP(4*($i+0),$T));
		&mov($B,&DWP(4*($i+1),$T));
		&mov($C,&DWP(4*($i+2),$T));
		&mov($D,&DWP(4*($i+3),$T));
		&bswap($A);
		&bswap($B);
		&bswap($C);
		&bswap($D);
		&mov(&swtmp($i+0),$A);
		&mov(&swtmp($i+1),$B);
		&mov(&swtmp($i+2),$C);
		&mov(&swtmp($i+3),$D);
		}
	&mov(&wparam(1),$T);	                       

	&mov($A,&DWP(0,$tmp1));	              
	&mov($B,&DWP(4,$tmp1));
	&mov($C,&DWP(8,$tmp1));
	&mov($D,&DWP(12,$tmp1));
	                 

	for($i=0;$i<16;$i++)	{ &BODY_00_15($i,@V); unshift(@V,pop(@V)); }
	for(;$i<20;$i++)	{ &BODY_16_19($i,@V); unshift(@V,pop(@V)); }
	for(;$i<40;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }
	for(;$i<60;$i++)	{ &BODY_40_59($i,@V); unshift(@V,pop(@V)); }
	for(;$i<80;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }

	(($V[5] eq $D) and ($V[0] eq $E)) or die;	              

	&mov($tmp1,&wparam(0));	                  
	&mov($D,&wparam(1));	                                

	&add($E,&DWP(0,$tmp1));	                  
	&add($T,&DWP(4,$tmp1));
	&add($A,&DWP(8,$tmp1));
	&add($B,&DWP(12,$tmp1));
	&add($C,&DWP(16,$tmp1));

	&mov(&DWP(0,$tmp1),$E);	                
	 &add($D,64);		                       
	&mov(&DWP(4,$tmp1),$T);
	 &cmp($D,&wparam(2));	                              
	&mov(&DWP(8,$tmp1),$A);
	 &mov($E,$C);		                                              
	&mov(&DWP(12,$tmp1),$B);
	 &mov($T,$D);		               
	&mov(&DWP(16,$tmp1),$C);
	&jb(&label("loop"));

	&stack_pop(16+3);
&function_end("sha1_block_data_order_nohw");

if ($xmm) {
if ($shaext) {
                                                                      
                                                              
 
my ($ctx,$inp,$num)=("edi","esi","ecx");
my ($ABCD,$E,$E_,$BSWAP)=map("xmm$_",(0..3));
my @MSG=map("xmm$_",(4..7));

sub sha1rnds4 {
 my ($dst,$src,$imm)=@_;
    if ("$dst:$src" =~ /xmm([0-7]):xmm([0-7])/)
    {	&data_byte(0x0f,0x3a,0xcc,0xc0|($1<<3)|$2,$imm);	}
}
sub sha1op38 {
 my ($opcodelet,$dst,$src)=@_;
    if ("$dst:$src" =~ /xmm([0-7]):xmm([0-7])/)
    {	&data_byte(0x0f,0x38,$opcodelet,0xc0|($1<<3)|$2);	}
}
sub sha1nexte	{ sha1op38(0xc8,@_); }
sub sha1msg1	{ sha1op38(0xc9,@_); }
sub sha1msg2	{ sha1op38(0xca,@_); }

&function_begin("sha1_block_data_order_shaext");
	&call	(&label("pic_point"));	              
	&set_label("pic_point");
	&blindpop($tmp1);
	&lea	($tmp1,&DWP(&label("K_XX_XX")."-".&label("pic_point"),$tmp1));
	&mov	($ctx,&wparam(0));
	&mov	("ebx","esp");
	&mov	($inp,&wparam(1));
	&mov	($num,&wparam(2));
	&sub	("esp",32);

	&movdqu	($ABCD,&QWP(0,$ctx));
	&movd	($E,&DWP(16,$ctx));
	&and	("esp",-32);
	&movdqa	($BSWAP,&QWP(0x50,$tmp1));	                  

	&movdqu	(@MSG[0],&QWP(0,$inp));
	&pshufd	($ABCD,$ABCD,0b00011011);	                 
	&movdqu	(@MSG[1],&QWP(0x10,$inp));
	&pshufd	($E,$E,0b00011011);		                 
	&movdqu	(@MSG[2],&QWP(0x20,$inp));
	&pshufb	(@MSG[0],$BSWAP);
	&movdqu	(@MSG[3],&QWP(0x30,$inp));
	&pshufb	(@MSG[1],$BSWAP);
	&pshufb	(@MSG[2],$BSWAP);
	&pshufb	(@MSG[3],$BSWAP);
	&jmp	(&label("loop_shaext"));

&set_label("loop_shaext",16);
	&dec		($num);
	&lea		("eax",&DWP(0x40,$inp));
	&movdqa		(&QWP(0,"esp"),$E);	            
	&paddd		($E,@MSG[0]);
	&cmovne		($inp,"eax");
	&movdqa		(&QWP(16,"esp"),$ABCD);	               

for($i=0;$i<20-4;$i+=2) {
	&sha1msg1	(@MSG[0],@MSG[1]);
	&movdqa		($E_,$ABCD);
	&sha1rnds4	($ABCD,$E,int($i/5));	        
	&sha1nexte	($E_,@MSG[1]);
	&pxor		(@MSG[0],@MSG[2]);
	&sha1msg1	(@MSG[1],@MSG[2]);
	&sha1msg2	(@MSG[0],@MSG[3]);

	&movdqa		($E,$ABCD);
	&sha1rnds4	($ABCD,$E_,int(($i+1)/5));
	&sha1nexte	($E,@MSG[2]);
	&pxor		(@MSG[1],@MSG[3]);
	&sha1msg2	(@MSG[1],@MSG[0]);

	push(@MSG,shift(@MSG));	push(@MSG,shift(@MSG));
}
	&movdqu		(@MSG[0],&QWP(0,$inp));
	&movdqa		($E_,$ABCD);
	&sha1rnds4	($ABCD,$E,3);		       
	&sha1nexte	($E_,@MSG[1]);
	&movdqu		(@MSG[1],&QWP(0x10,$inp));
	&pshufb		(@MSG[0],$BSWAP);

	&movdqa		($E,$ABCD);
	&sha1rnds4	($ABCD,$E_,3);		       
	&sha1nexte	($E,@MSG[2]);
	&movdqu		(@MSG[2],&QWP(0x20,$inp));
	&pshufb		(@MSG[1],$BSWAP);

	&movdqa		($E_,$ABCD);
	&sha1rnds4	($ABCD,$E,3);		       
	&sha1nexte	($E_,@MSG[3]);
	&movdqu		(@MSG[3],&QWP(0x30,$inp));
	&pshufb		(@MSG[2],$BSWAP);

	&movdqa		($E,$ABCD);
	&sha1rnds4	($ABCD,$E_,3);		       
	&movdqa		($E_,&QWP(0,"esp"));
	&pshufb		(@MSG[3],$BSWAP);
	&sha1nexte	($E,$E_);
	&paddd		($ABCD,&QWP(16,"esp"));

	&jnz		(&label("loop_shaext"));

	&pshufd	($ABCD,$ABCD,0b00011011);
	&pshufd	($E,$E,0b00011011);
	&movdqu	(&QWP(0,$ctx),$ABCD)
	&movd	(&DWP(16,$ctx),$E);
	&mov	("esp","ebx");
&function_end("sha1_block_data_order_shaext");
}
                                                                      
                           
 
                                                                      
                                                                 
                                                                 
                                                                 
                                                                   
                                                                
 
                                                                    
                                                                   
                                                                   
                                                                    
                            
 
                                                                    
                                                     
 
                                                                     
                                                                    
                                                         
 
my $Xi=4;			                                        
my @X=map("xmm$_",(4..7,0..3));	                      
my @V=($A,$B,$C,$D,$E);
my $j=0;			            
my $rx=0;
my @T=($T,$tmp1);
my $inp;

my $_rol=sub { &rol(@_) };
my $_ror=sub { &ror(@_) };

&function_begin("sha1_block_data_order_ssse3");
	&call	(&label("pic_point"));	              
	&set_label("pic_point");
	&blindpop($tmp1);
	&lea	($tmp1,&DWP(&label("K_XX_XX")."-".&label("pic_point"),$tmp1));

	&movdqa	(@X[3],&QWP(0,$tmp1));		         
	&movdqa	(@X[4],&QWP(16,$tmp1));		         
	&movdqa	(@X[5],&QWP(32,$tmp1));		         
	&movdqa	(@X[6],&QWP(48,$tmp1));		         
	&movdqa	(@X[2],&QWP(64,$tmp1));		             

	&mov	($E,&wparam(0));		                     
	&mov	($inp=@T[1],&wparam(1));
	&mov	($D,&wparam(2));
	&mov	(@T[0],"esp");

	                    
	 
	                                                      
	                             
	                               
	                                 
	 
	                                                   
	                     
	                                                   
	 
	                                                  
	                                 
	                                 
	                                 
	             
	 
	                              
	          
	          
	          
	&sub	("esp",208);
	&and	("esp",-64);

	&movdqa	(&QWP(112+0,"esp"),@X[4]);	                
	&movdqa	(&QWP(112+16,"esp"),@X[5]);
	&movdqa	(&QWP(112+32,"esp"),@X[6]);
	&shl	($D,6);				        
	&movdqa	(&QWP(112+48,"esp"),@X[3]);
	&add	($D,$inp);			              
	&movdqa	(&QWP(112+64,"esp"),@X[2]);
	&add	($inp,64);
	&mov	(&DWP(192+0,"esp"),$E);		                     
	&mov	(&DWP(192+4,"esp"),$inp);
	&mov	(&DWP(192+8,"esp"),$D);
	&mov	(&DWP(192+12,"esp"),@T[0]);	                    

	&mov	($A,&DWP(0,$E));		              
	&mov	($B,&DWP(4,$E));
	&mov	($C,&DWP(8,$E));
	&mov	($D,&DWP(12,$E));
	&mov	($E,&DWP(16,$E));
	&mov	(@T[0],$B);			            

	&movdqu	(@X[-4&7],&QWP(-64,$inp));	                         
	&movdqu	(@X[-3&7],&QWP(-48,$inp));
	&movdqu	(@X[-2&7],&QWP(-32,$inp));
	&movdqu	(@X[-1&7],&QWP(-16,$inp));
	&pshufb	(@X[-4&7],@X[2]);		           
	&pshufb	(@X[-3&7],@X[2]);
	&pshufb	(@X[-2&7],@X[2]);
	&movdqa	(&QWP(112-16,"esp"),@X[3]);	                            
	&pshufb	(@X[-1&7],@X[2]);
	&paddd	(@X[-4&7],@X[3]);		             
	&paddd	(@X[-3&7],@X[3]);
	&paddd	(@X[-2&7],@X[3]);
	&movdqa	(&QWP(0,"esp"),@X[-4&7]);	                    
	&psubd	(@X[-4&7],@X[3]);		             
	&movdqa	(&QWP(0+16,"esp"),@X[-3&7]);
	&psubd	(@X[-3&7],@X[3]);
	&movdqa	(&QWP(0+32,"esp"),@X[-2&7]);
	&mov	(@T[1],$C);
	&psubd	(@X[-2&7],@X[3]);
	&xor	(@T[1],$D);
	&pshufd	(@X[0],@X[-4&7],0xee);		                               
	&and	(@T[0],@T[1]);
	&jmp	(&label("loop"));

                                                                      
                                                                   
                                                                  
                                                                    
                                                                     
                                                                     
                                            
 
                                                                     
                                                                 
                                                                   
                                                                  
                                                                     
                                                                     
                                                                
           
 
sub Xupdate_ssse3_16_31()		                               
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns));
	&punpcklqdq(@X[0],@X[-3&7]);	                                                             
	&movdqa	(@X[2],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &paddd	(@X[3],@X[-1&7]);
	  &movdqa	(&QWP(64+16*(($Xi-4)%3),"esp"),@X[-4&7]);                              
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	&psrldq	(@X[2],4);		                   
	 eval(shift(@insns));
	 eval(shift(@insns));
	&pxor	(@X[0],@X[-4&7]);	                  
	 eval(shift(@insns));
	 eval(shift(@insns));		     

	&pxor	(@X[2],@X[-2&7]);	                 
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pxor	(@X[0],@X[2]);		                         
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	  &movdqa	(&QWP(0+16*(($Xi-1)&3),"esp"),@X[3]);	                    
	 eval(shift(@insns));
	 eval(shift(@insns));

	&movdqa	(@X[4],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	&movdqa (@X[2],@X[0]);
	 eval(shift(@insns));

	&pslldq	(@X[4],12);		                               
	&paddd	(@X[0],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));

	&psrld	(@X[2],31);
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	&movdqa	(@X[3],@X[4]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&psrld	(@X[4],30);
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	&por	(@X[0],@X[2]);		             
	 eval(shift(@insns));
	  &movdqa	(@X[2],&QWP(64+16*(($Xi-6)%3),"esp")) if ($Xi>5);	                                   
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pslld	(@X[3],2);
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	&pxor   (@X[0],@X[4]);
	  &movdqa	(@X[4],&QWP(112-16+16*(($Xi)/5),"esp"));	         
	 eval(shift(@insns));
	 eval(shift(@insns));

	&pxor	(@X[0],@X[3]);		                          
	  &pshufd	(@X[1],@X[-3&7],0xee)	if ($Xi<7);	                              
	  &pshufd	(@X[3],@X[-1&7],0xee)	if ($Xi==7);
	 eval(shift(@insns));
	 eval(shift(@insns));

	 foreach (@insns) { eval; }	                                 

  $Xi++;	push(@X,shift(@X));	              
}

sub Xupdate_ssse3_32_79()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                       
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));		            
	&pxor	(@X[0],@X[-4&7]);	                          
	&punpcklqdq(@X[2],@X[-1&7]);	                                                 
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     

	&pxor	(@X[0],@X[-7&7]);	                  
	  &movdqa	(&QWP(64+16*(($Xi-4)%3),"esp"),@X[-4&7]);	                              
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns))		if (@insns[0] =~ /_rol/);
	 if ($Xi%5) {
	  &movdqa	(@X[4],@X[3]);	                         
	 } else {			                      
	  &movdqa	(@X[4],&QWP(112-16+16*($Xi/5),"esp"));
	 }
	 eval(shift(@insns));		     
	  &paddd	(@X[3],@X[-1&7]);
	 eval(shift(@insns));

	&pxor	(@X[0],@X[2]);		                 
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     

	&movdqa	(@X[2],@X[0]);
	  &movdqa	(&QWP(0+16*(($Xi-1)&3),"esp"),@X[3]);	                    
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns))		if (@insns[0] =~ /_rol/);

	&pslld	(@X[0],2);
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	&psrld	(@X[2],30);
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns))		if (@insns[1] =~ /_rol/);
	 eval(shift(@insns))		if (@insns[0] =~ /_rol/);

	&por	(@X[0],@X[2]);		             
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	  &movdqa	(@X[2],&QWP(64+16*(($Xi-6)%3),"esp")) if($Xi<19);	                                   
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	  &pshufd	(@X[3],@X[-1],0xee)	if ($Xi<19);	                           
	 eval(shift(@insns));

	 foreach (@insns) { eval; }	                        

  $Xi++;	push(@X,shift(@X));	              
}

sub Xuplast_ssse3_80()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &paddd	(@X[3],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &movdqa	(&QWP(0+16*(($Xi-1)&3),"esp"),@X[3]);	                 

	 foreach (@insns) { eval; }		                        

	&mov	($inp=@T[1],&DWP(192+4,"esp"));
	&cmp	($inp,&DWP(192+8,"esp"));
	&je	(&label("done"));

	&movdqa	(@X[3],&QWP(112+48,"esp"));	         
	&movdqa	(@X[2],&QWP(112+64,"esp"));	             
	&movdqu	(@X[-4&7],&QWP(0,$inp));	            
	&movdqu	(@X[-3&7],&QWP(16,$inp));
	&movdqu	(@X[-2&7],&QWP(32,$inp));
	&movdqu	(@X[-1&7],&QWP(48,$inp));
	&add	($inp,64);
	&pshufb	(@X[-4&7],@X[2]);		           
	&mov	(&DWP(192+4,"esp"),$inp);
	&movdqa	(&QWP(112-16,"esp"),@X[3]);	                            

  $Xi=0;
}

sub Xloop_ssse3()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&pshufb	(@X[($Xi-3)&7],@X[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&paddd	(@X[($Xi-4)&7],@X[3]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&movdqa	(&QWP(0+16*$Xi,"esp"),@X[($Xi-4)&7]);	                    
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&psubd	(@X[($Xi-4)&7],@X[3]);

	foreach (@insns) { eval; }
  $Xi++;
}

sub Xtail_ssse3()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	foreach (@insns) { eval; }
}

sub body_00_19 () {	             
	                        
	return &body_20_39()	if ($rx==19);	$rx++;
	(
	'($a,$b,$c,$d,$e)=@V;'.
	'&$_ror	($b,$j?7:2);',	        
	'&xor	(@T[0],$d);',
	'&mov	(@T[1],$a);',	                  

	'&add	($e,&DWP(4*($j&15),"esp"));',	            
	'&xor	($b,$c);',	                      

	'&$_rol	($a,5);',
	'&add	($e,@T[0]);',
	'&and	(@T[1],$b);',	                             

	'&xor	($b,$c);',	            
	'&add	($e,$a);'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
}

sub body_20_39 () {	       
	                    
	return &body_40_59()	if ($rx==39);	$rx++;
	(
	'($a,$b,$c,$d,$e)=@V;'.
	'&add	($e,&DWP(4*($j&15),"esp"));',	            
	'&xor	(@T[0],$d)	if($j==19);'.
	'&xor	(@T[0],$c)	if($j> 19);',	            
	'&mov	(@T[1],$a);',	                  

	'&$_rol	($a,5);',
	'&add	($e,@T[0]);',
	'&xor	(@T[1],$c)	if ($j< 79);',	                      

	'&$_ror	($b,7);',	        
	'&add	($e,$a);'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
}

sub body_40_59 () {	                 
	                              
	$rx++;
	(
	'($a,$b,$c,$d,$e)=@V;'.
	'&add	($e,&DWP(4*($j&15),"esp"));',	            
	'&and	(@T[0],$c)	if ($j>=40);',	             
	'&xor	($c,$d)		if ($j>=40);',	            

	'&$_ror	($b,7);',	        
	'&mov	(@T[1],$a);',	                   
	'&xor	(@T[0],$c);',

	'&$_rol	($a,5);',
	'&add	($e,@T[0]);',
	'&xor	(@T[1],$c)	if ($j==59);'.
	'&xor	(@T[1],$b)	if ($j< 59);',	                    

	'&xor	($b,$c)		if ($j< 59);',	                    
	'&add	($e,$a);'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
}
      
sub bodyx_00_19 () {	             
	                                        
	return &bodyx_20_39()	if ($rx==19);	$rx++;
	(
	'($a,$b,$c,$d,$e)=@V;'.

	'&rorx	($b,$b,2)			if ($j==0);'.	        
	'&rorx	($b,@T[1],7)			if ($j!=0);',	        
	'&lea	($e,&DWP(0,$e,@T[0]));',
	'&rorx	(@T[0],$a,5);',

	'&andn	(@T[1],$a,$c);',
	'&and	($a,$b)',
	'&add	($d,&DWP(4*(($j+1)&15),"esp"));',	            

	'&xor	(@T[1],$a)',
	'&add	($e,@T[0]);'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
}

sub bodyx_20_39 () {	       
	                   
	return &bodyx_40_59()	if ($rx==39);	$rx++;
	(
	'($a,$b,$c,$d,$e)=@V;'.

	'&add	($e,($j==19?@T[0]:$b))',
	'&rorx	($b,@T[1],7);',	        
	'&rorx	(@T[0],$a,5);',

	'&xor	($a,$b)				if ($j<79);',
	'&add	($d,&DWP(4*(($j+1)&15),"esp"))	if ($j<79);',	            
	'&xor	($a,$c)				if ($j<79);',
	'&add	($e,@T[0]);'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
}

sub bodyx_40_59 () {	                 
	                             
	return &bodyx_20_39()	if ($rx==59);	$rx++;
	(
	'($a,$b,$c,$d,$e)=@V;'.

	'&rorx	(@T[0],$a,5)',
	'&lea	($e,&DWP(0,$e,$b))',
	'&rorx	($b,@T[1],7)',	        
	'&add	($d,&DWP(4*(($j+1)&15),"esp"))',	            

	'&mov	(@T[1],$c)',
	'&xor	($a,$b)',	                    
	'&xor	(@T[1],$b)',	                    

	'&and	($a,@T[1])',
	'&add	($e,@T[0])',
	'&xor	($a,$b)'	.'$j++; unshift(@V,pop(@V)); unshift(@T,pop(@T));'
	);
}

&set_label("loop",16);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_16_31(\&body_00_19);
	&Xupdate_ssse3_32_79(\&body_00_19);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_40_59);
	&Xupdate_ssse3_32_79(\&body_20_39);
	&Xuplast_ssse3_80(\&body_20_39);	                    

				$saved_j=$j; @saved_V=@V;

	&Xloop_ssse3(\&body_20_39);
	&Xloop_ssse3(\&body_20_39);
	&Xloop_ssse3(\&body_20_39);

	&mov	(@T[1],&DWP(192,"esp"));	                
	&add	($A,&DWP(0,@T[1]));
	&add	(@T[0],&DWP(4,@T[1]));		    
	&add	($C,&DWP(8,@T[1]));
	&mov	(&DWP(0,@T[1]),$A);
	&add	($D,&DWP(12,@T[1]));
	&mov	(&DWP(4,@T[1]),@T[0]);
	&add	($E,&DWP(16,@T[1]));
	&mov	(&DWP(8,@T[1]),$C);
	&mov	($B,$C);
	&mov	(&DWP(12,@T[1]),$D);
	&xor	($B,$D);
	&mov	(&DWP(16,@T[1]),$E);
	&mov	(@T[1],@T[0]);
	&pshufd	(@X[0],@X[-4&7],0xee);		                               
	&and	(@T[0],$B);
	&mov	($B,$T[1]);

	&jmp	(&label("loop"));

&set_label("done",16);		$j=$saved_j; @V=@saved_V;

	&Xtail_ssse3(\&body_20_39);
	&Xtail_ssse3(\&body_20_39);
	&Xtail_ssse3(\&body_20_39);

	&mov	(@T[1],&DWP(192,"esp"));	                
	&add	($A,&DWP(0,@T[1]));
	&mov	("esp",&DWP(192+12,"esp"));	              
	&add	(@T[0],&DWP(4,@T[1]));		    
	&add	($C,&DWP(8,@T[1]));
	&mov	(&DWP(0,@T[1]),$A);
	&add	($D,&DWP(12,@T[1]));
	&mov	(&DWP(4,@T[1]),@T[0]);
	&add	($E,&DWP(16,@T[1]));
	&mov	(&DWP(8,@T[1]),$C);
	&mov	(&DWP(12,@T[1]),$D);
	&mov	(&DWP(16,@T[1]),$E);

&function_end("sha1_block_data_order_ssse3");

$rx=0;	       

if ($ymm) {
my $Xi=4;			                                        
my @X=map("xmm$_",(4..7,0..3));	                      
my @V=($A,$B,$C,$D,$E);
my $j=0;			            
my @T=($T,$tmp1);
my $inp;

my $_rol=sub { &shld(@_[0],@_) };
my $_ror=sub { &shrd(@_[0],@_) };

&function_begin("sha1_block_data_order_avx");
	&call	(&label("pic_point"));	              
	&set_label("pic_point");
	&blindpop($tmp1);
	&lea	($tmp1,&DWP(&label("K_XX_XX")."-".&label("pic_point"),$tmp1));
	&vzeroall();

	&vmovdqa(@X[3],&QWP(0,$tmp1));		         
	&vmovdqa(@X[4],&QWP(16,$tmp1));		         
	&vmovdqa(@X[5],&QWP(32,$tmp1));		         
	&vmovdqa(@X[6],&QWP(48,$tmp1));		         
	&vmovdqa(@X[2],&QWP(64,$tmp1));		             

	&mov	($E,&wparam(0));		                     
	&mov	($inp=@T[1],&wparam(1));
	&mov	($D,&wparam(2));
	&mov	(@T[0],"esp");

	                    
	 
	                                                      
	                             
	                               
	                                 
	 
	                                                   
	                     
	                                                   
	 
	                                                  
	                                 
	                                 
	                                 
	             
	 
	                              
	          
	          
	          
	&sub	("esp",208);
	&and	("esp",-64);

	&vmovdqa(&QWP(112+0,"esp"),@X[4]);	                
	&vmovdqa(&QWP(112+16,"esp"),@X[5]);
	&vmovdqa(&QWP(112+32,"esp"),@X[6]);
	&shl	($D,6);				        
	&vmovdqa(&QWP(112+48,"esp"),@X[3]);
	&add	($D,$inp);			              
	&vmovdqa(&QWP(112+64,"esp"),@X[2]);
	&add	($inp,64);
	&mov	(&DWP(192+0,"esp"),$E);		                     
	&mov	(&DWP(192+4,"esp"),$inp);
	&mov	(&DWP(192+8,"esp"),$D);
	&mov	(&DWP(192+12,"esp"),@T[0]);	                    

	&mov	($A,&DWP(0,$E));		              
	&mov	($B,&DWP(4,$E));
	&mov	($C,&DWP(8,$E));
	&mov	($D,&DWP(12,$E));
	&mov	($E,&DWP(16,$E));
	&mov	(@T[0],$B);			            

	&vmovdqu(@X[-4&7],&QWP(-64,$inp));	                         
	&vmovdqu(@X[-3&7],&QWP(-48,$inp));
	&vmovdqu(@X[-2&7],&QWP(-32,$inp));
	&vmovdqu(@X[-1&7],&QWP(-16,$inp));
	&vpshufb(@X[-4&7],@X[-4&7],@X[2]);	           
	&vpshufb(@X[-3&7],@X[-3&7],@X[2]);
	&vpshufb(@X[-2&7],@X[-2&7],@X[2]);
	&vmovdqa(&QWP(112-16,"esp"),@X[3]);	                            
	&vpshufb(@X[-1&7],@X[-1&7],@X[2]);
	&vpaddd	(@X[0],@X[-4&7],@X[3]);		             
	&vpaddd	(@X[1],@X[-3&7],@X[3]);
	&vpaddd	(@X[2],@X[-2&7],@X[3]);
	&vmovdqa(&QWP(0,"esp"),@X[0]);		                    
	&mov	(@T[1],$C);
	&vmovdqa(&QWP(0+16,"esp"),@X[1]);
	&xor	(@T[1],$D);
	&vmovdqa(&QWP(0+32,"esp"),@X[2]);
	&and	(@T[0],@T[1]);
	&jmp	(&label("loop"));

sub Xupdate_avx_16_31()		                               
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpalignr(@X[0],@X[-3&7],@X[-4&7],8);	                            
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &vpaddd	(@X[3],@X[3],@X[-1&7]);
	  &vmovdqa	(&QWP(64+16*(($Xi-4)%3),"esp"),@X[-4&7]);                              
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpsrldq(@X[2],@X[-1&7],4);		                   
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpxor	(@X[0],@X[0],@X[-4&7]);		                  
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpxor	(@X[2],@X[2],@X[-2&7]);		                 
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vmovdqa	(&QWP(0+16*(($Xi-1)&3),"esp"),@X[3]);	                    
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpxor	(@X[0],@X[0],@X[2]);		                         
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpsrld	(@X[2],@X[0],31);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpslldq(@X[4],@X[0],12);		                               
	&vpaddd	(@X[0],@X[0],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpsrld	(@X[3],@X[4],30);
	&vpor	(@X[0],@X[0],@X[2]);		             
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpslld	(@X[4],@X[4],2);
	  &vmovdqa	(@X[2],&QWP(64+16*(($Xi-6)%3),"esp")) if ($Xi>5);	                                   
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpxor	(@X[0],@X[0],@X[3]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	&vpxor	(@X[0],@X[0],@X[4]);		                          
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vmovdqa	(@X[4],&QWP(112-16+16*(($Xi)/5),"esp"));	         
	 eval(shift(@insns));
	 eval(shift(@insns));

	 foreach (@insns) { eval; }	                                 

  $Xi++;	push(@X,shift(@X));	              
}

sub Xupdate_avx_32_79()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                       
  my ($a,$b,$c,$d,$e);

	&vpalignr(@X[2],@X[-1&7],@X[-2&7],8);	                 
	&vpxor	(@X[0],@X[0],@X[-4&7]);	                          
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     

	&vpxor	(@X[0],@X[0],@X[-7&7]);	                  
	  &vmovdqa	(&QWP(64+16*(($Xi-4)%3),"esp"),@X[-4&7]);	                              
	 eval(shift(@insns));
	 eval(shift(@insns));
	 if ($Xi%5) {
	  &vmovdqa	(@X[4],@X[3]);	                         
	 } else {			                      
	  &vmovdqa	(@X[4],&QWP(112-16+16*($Xi/5),"esp"));
	 }
	  &vpaddd	(@X[3],@X[3],@X[-1&7]);
	 eval(shift(@insns));		     
	 eval(shift(@insns));

	&vpxor	(@X[0],@X[0],@X[2]);		                 
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     

	&vpsrld	(@X[2],@X[0],30);
	  &vmovdqa	(&QWP(0+16*(($Xi-1)&3),"esp"),@X[3]);	                    
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));

	&vpslld	(@X[0],@X[0],2);
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));

	&vpor	(@X[0],@X[0],@X[2]);	             
	 eval(shift(@insns));		            
	 eval(shift(@insns));
	  &vmovdqa	(@X[2],&QWP(64+16*(($Xi-6)%3),"esp")) if($Xi<19);	                                   
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));		     
	 eval(shift(@insns));

	 foreach (@insns) { eval; }	                        

  $Xi++;	push(@X,shift(@X));	              
}

sub Xuplast_avx_80()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	  &vpaddd	(@X[3],@X[3],@X[-1&7]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));

	  &vmovdqa	(&QWP(0+16*(($Xi-1)&3),"esp"),@X[3]);	                 

	 foreach (@insns) { eval; }		                        

	&mov	($inp=@T[1],&DWP(192+4,"esp"));
	&cmp	($inp,&DWP(192+8,"esp"));
	&je	(&label("done"));

	&vmovdqa(@X[3],&QWP(112+48,"esp"));	         
	&vmovdqa(@X[2],&QWP(112+64,"esp"));	             
	&vmovdqu(@X[-4&7],&QWP(0,$inp));	            
	&vmovdqu(@X[-3&7],&QWP(16,$inp));
	&vmovdqu(@X[-2&7],&QWP(32,$inp));
	&vmovdqu(@X[-1&7],&QWP(48,$inp));
	&add	($inp,64);
	&vpshufb(@X[-4&7],@X[-4&7],@X[2]);		           
	&mov	(&DWP(192+4,"esp"),$inp);
	&vmovdqa(&QWP(112-16,"esp"),@X[3]);	                            

  $Xi=0;
}

sub Xloop_avx()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpshufb	(@X[($Xi-3)&7],@X[($Xi-3)&7],@X[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vpaddd	(@X[$Xi&7],@X[($Xi-4)&7],@X[3]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vmovdqa	(&QWP(0+16*$Xi,"esp"),@X[$Xi&7]);	                    
	 eval(shift(@insns));
	 eval(shift(@insns));

	foreach (@insns) { eval; }
  $Xi++;
}

sub Xtail_avx()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);	                 
  my ($a,$b,$c,$d,$e);

	foreach (@insns) { eval; }
}

&set_label("loop",16);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_16_31(\&body_00_19);
	&Xupdate_avx_32_79(\&body_00_19);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_40_59);
	&Xupdate_avx_32_79(\&body_20_39);
	&Xuplast_avx_80(\&body_20_39);	                    

				$saved_j=$j; @saved_V=@V;

	&Xloop_avx(\&body_20_39);
	&Xloop_avx(\&body_20_39);
	&Xloop_avx(\&body_20_39);

	&mov	(@T[1],&DWP(192,"esp"));	                
	&add	($A,&DWP(0,@T[1]));
	&add	(@T[0],&DWP(4,@T[1]));		    
	&add	($C,&DWP(8,@T[1]));
	&mov	(&DWP(0,@T[1]),$A);
	&add	($D,&DWP(12,@T[1]));
	&mov	(&DWP(4,@T[1]),@T[0]);
	&add	($E,&DWP(16,@T[1]));
	&mov	($B,$C);
	&mov	(&DWP(8,@T[1]),$C);
	&xor	($B,$D);
	&mov	(&DWP(12,@T[1]),$D);
	&mov	(&DWP(16,@T[1]),$E);
	&mov	(@T[1],@T[0]);
	&and	(@T[0],$B);
	&mov	($B,@T[1]);

	&jmp	(&label("loop"));

&set_label("done",16);		$j=$saved_j; @V=@saved_V;

	&Xtail_avx(\&body_20_39);
	&Xtail_avx(\&body_20_39);
	&Xtail_avx(\&body_20_39);

	&vzeroall();

	&mov	(@T[1],&DWP(192,"esp"));	                
	&add	($A,&DWP(0,@T[1]));
	&mov	("esp",&DWP(192+12,"esp"));	              
	&add	(@T[0],&DWP(4,@T[1]));		    
	&add	($C,&DWP(8,@T[1]));
	&mov	(&DWP(0,@T[1]),$A);
	&add	($D,&DWP(12,@T[1]));
	&mov	(&DWP(4,@T[1]),@T[0]);
	&add	($E,&DWP(16,@T[1]));
	&mov	(&DWP(8,@T[1]),$C);
	&mov	(&DWP(12,@T[1]),$D);
	&mov	(&DWP(16,@T[1]),$E);
&function_end("sha1_block_data_order_avx");
}
&set_label("K_XX_XX",64);
&data_word(0x5a827999,0x5a827999,0x5a827999,0x5a827999);	         
&data_word(0x6ed9eba1,0x6ed9eba1,0x6ed9eba1,0x6ed9eba1);	         
&data_word(0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc);	         
&data_word(0xca62c1d6,0xca62c1d6,0xca62c1d6,0xca62c1d6);	         
&data_word(0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f);	             
&data_byte(0xf,0xe,0xd,0xc,0xb,0xa,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0);
}
&asciz("SHA1 block transform for x86, CRYPTOGAMS by <appro\@openssl.org>");

&asm_finish();

close STDOUT or die "error closing STDOUT: $!";
