#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
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

#
# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project.
# ====================================================================
#
# January 2015
#
# ChaCha20 for x86.
#
# Performance in cycles per byte out of large buffer.
#
#		1xIALU/gcc	4xSSSE3
# Pentium	17.5/+80%
# PIII		14.2/+60%
# P4		18.6/+84%
# Core2		9.56/+89%	4.83
# Westmere	9.50/+45%	3.35
# Sandy Bridge	10.5/+47%	3.20
# Haswell	8.15/+50%	2.83
# Skylake	7.53/+22%	2.75
# Silvermont	17.4/+36%	8.35
# Goldmont	13.4/+40%	4.36
# Sledgehammer	10.2/+54%
# Bulldozer	13.4/+50%	4.38(*)
#
# (*)	Bulldozer actually executes 4xXOP code path that delivers 3.55;
#
# Modified from upstream OpenSSL to remove the XOP code.

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC,"${dir}","${dir}../../perlasm");
require "x86asm.pl";

$output=pop;
open STDOUT,">$output";

&asm_init($ARGV[0]);

$xmm=$ymm=1;
$gasver=999;                     

$a="eax";
($b,$b_)=("ebx","ebp");
($c,$c_)=("ecx","esi");
($d,$d_)=("edx","edi");

sub QUARTERROUND {
my ($ai,$bi,$ci,$di,$i)=@_;
my ($an,$bn,$cn,$dn)=map(($_&~3)+(($_+1)&3),($ai,$bi,$ci,$di));	      
my ($ap,$bp,$cp,$dp)=map(($_&~3)+(($_-1)&3),($ai,$bi,$ci,$di));	          

	                     
	 
	                                  
	                     
	                     
	                     
	                                 
	                     
	                     
	                     

	if ($i==0) {
            my $j=4;
	    ($ap,$bp,$cp,$dp)=map(($_&~3)+(($_-$j--)&3),($ap,$bp,$cp,$dp));
	} elsif ($i==3) {
            my $j=0;
	    ($an,$bn,$cn,$dn)=map(($_&~3)+(($_+$j++)&3),($an,$bn,$cn,$dn));
	} elsif ($i==4) {
            my $j=4;
	    ($ap,$bp,$cp,$dp)=map(($_&~3)+(($_+$j--)&3),($ap,$bp,$cp,$dp));
	} elsif ($i==7) {
            my $j=0;
	    ($an,$bn,$cn,$dn)=map(($_&~3)+(($_-$j++)&3),($an,$bn,$cn,$dn));
	}

	                                
	&xor	($d,$a);
	 &mov	(&DWP(4*$cp,"esp"),$c_)		if ($ai>0 && $ai<3);
	&rol	($d,16);
	 &mov	(&DWP(4*$bp,"esp"),$b_)		if ($i!=0);
	&add	($c,$d);
	 &mov	($c_,&DWP(4*$cn,"esp"))		if ($ai>0 && $ai<3);
	&xor	($b,$c);
	 &mov	($d_,&DWP(4*$dn,"esp"))		if ($di!=$dn);
	&rol	($b,12);
	 &mov	($b_,&DWP(4*$bn,"esp"))		if ($i<7);
	 &mov	($b_,&DWP(128,"esp"))		if ($i==7);	              
	&add	($a,$b);
	&xor	($d,$a);
	&mov	(&DWP(4*$ai,"esp"),$a);
	&rol	($d,8);
	&mov	($a,&DWP(4*$an,"esp"));
	&add	($c,$d);
	&mov	(&DWP(4*$di,"esp"),$d)		if ($di!=$dn);
	&mov	($d_,$d)			if ($di==$dn);
	&xor	($b,$c);
	 &add	($a,$b_)			if ($i<7);	           
	&rol	($b,7);

	($b,$b_)=($b_,$b);
	($c,$c_)=($c_,$c);
	($d,$d_)=($d_,$d);
}

&static_label("ssse3_data");
&static_label("pic_point");

&function_begin("ChaCha20_ctr32_nohw");
	&mov	("esi",&wparam(3));		     
	&mov	("edi",&wparam(4));		                   

	&stack_push(33);

	&mov	("eax",&DWP(4*0,"esi"));	          
	&mov	("ebx",&DWP(4*1,"esi"));
	&mov	("ecx",&DWP(4*2,"esi"));
	&mov	("edx",&DWP(4*3,"esi"));
	&mov	(&DWP(64+4*4,"esp"),"eax");
	&mov	(&DWP(64+4*5,"esp"),"ebx");
	&mov	(&DWP(64+4*6,"esp"),"ecx");
	&mov	(&DWP(64+4*7,"esp"),"edx");
	&mov	("eax",&DWP(4*4,"esi"));
	&mov	("ebx",&DWP(4*5,"esi"));
	&mov	("ecx",&DWP(4*6,"esi"));
	&mov	("edx",&DWP(4*7,"esi"));
	&mov	(&DWP(64+4*8,"esp"),"eax");
	&mov	(&DWP(64+4*9,"esp"),"ebx");
	&mov	(&DWP(64+4*10,"esp"),"ecx");
	&mov	(&DWP(64+4*11,"esp"),"edx");
	&mov	("eax",&DWP(4*0,"edi"));	                        
	&mov	("ebx",&DWP(4*1,"edi"));
	&mov	("ecx",&DWP(4*2,"edi"));
	&mov	("edx",&DWP(4*3,"edi"));
	&sub	("eax",1);
	&mov	(&DWP(64+4*12,"esp"),"eax");
	&mov	(&DWP(64+4*13,"esp"),"ebx");
	&mov	(&DWP(64+4*14,"esp"),"ecx");
	&mov	(&DWP(64+4*15,"esp"),"edx");
	&jmp	(&label("entry"));

&set_label("outer_loop",16);
	&mov	(&wparam(1),$b);		            
	&mov	(&wparam(0),$a);		             
	&mov	(&wparam(2),$c);		          
&set_label("entry");
	&mov	($a,0x61707865);
	&mov	(&DWP(4*1,"esp"),0x3320646e);
	&mov	(&DWP(4*2,"esp"),0x79622d32);
	&mov	(&DWP(4*3,"esp"),0x6b206574);

	&mov	($b, &DWP(64+4*5,"esp"));	                   
	&mov	($b_,&DWP(64+4*6,"esp"));
	&mov	($c, &DWP(64+4*10,"esp"));
	&mov	($c_,&DWP(64+4*11,"esp"));
	&mov	($d, &DWP(64+4*13,"esp"));
	&mov	($d_,&DWP(64+4*14,"esp"));
	&mov	(&DWP(4*5,"esp"),$b);
	&mov	(&DWP(4*6,"esp"),$b_);
	&mov	(&DWP(4*10,"esp"),$c);
	&mov	(&DWP(4*11,"esp"),$c_);
	&mov	(&DWP(4*13,"esp"),$d);
	&mov	(&DWP(4*14,"esp"),$d_);

	&mov	($b, &DWP(64+4*7,"esp"));
	&mov	($d_,&DWP(64+4*15,"esp"));
	&mov	($d, &DWP(64+4*12,"esp"));
	&mov	($b_,&DWP(64+4*4,"esp"));
	&mov	($c, &DWP(64+4*8,"esp"));
	&mov	($c_,&DWP(64+4*9,"esp"));
	&add	($d,1);				               
	&mov	(&DWP(4*7,"esp"),$b);
	&mov	(&DWP(4*15,"esp"),$d_);
	&mov	(&DWP(64+4*12,"esp"),$d);	                    

	&mov	($b,10);			              
	&jmp	(&label("loop"));

&set_label("loop",16);
	&add	($a,$b_);			           
	&mov	(&DWP(128,"esp"),$b);		                   
	&mov	($b,$b_);
	&QUARTERROUND(0, 4, 8, 12, 0);
	&QUARTERROUND(1, 5, 9, 13, 1);
	&QUARTERROUND(2, 6,10, 14, 2);
	&QUARTERROUND(3, 7,11, 15, 3);
	&QUARTERROUND(0, 5,10, 15, 4);
	&QUARTERROUND(1, 6,11, 12, 5);
	&QUARTERROUND(2, 7, 8, 13, 6);
	&QUARTERROUND(3, 4, 9, 14, 7);
	&dec	($b);
	&jnz	(&label("loop"));

	&mov	($b,&wparam(2));		          

	&add	($a,0x61707865);		                         
	&add	($b_,&DWP(64+4*4,"esp"));
	&add	($c, &DWP(64+4*8,"esp"));
	&add	($c_,&DWP(64+4*9,"esp"));

	&cmp	($b,64);
	&jb	(&label("tail"));

	&mov	($b,&wparam(1));		                    
	&add	($d, &DWP(64+4*12,"esp"));
	&add	($d_,&DWP(64+4*14,"esp"));

	&xor	($a, &DWP(4*0,$b));		                
	&xor	($b_,&DWP(4*4,$b));
	&mov	(&DWP(4*0,"esp"),$a);
	&mov	($a,&wparam(0));		                     
	&xor	($c, &DWP(4*8,$b));
	&xor	($c_,&DWP(4*9,$b));
	&xor	($d, &DWP(4*12,$b));
	&xor	($d_,&DWP(4*14,$b));
	&mov	(&DWP(4*4,$a),$b_);		              
	&mov	(&DWP(4*8,$a),$c);
	&mov	(&DWP(4*9,$a),$c_);
	&mov	(&DWP(4*12,$a),$d);
	&mov	(&DWP(4*14,$a),$d_);

	&mov	($b_,&DWP(4*1,"esp"));
	&mov	($c, &DWP(4*2,"esp"));
	&mov	($c_,&DWP(4*3,"esp"));
	&mov	($d, &DWP(4*5,"esp"));
	&mov	($d_,&DWP(4*6,"esp"));
	&add	($b_,0x3320646e);		                         
	&add	($c, 0x79622d32);
	&add	($c_,0x6b206574);
	&add	($d, &DWP(64+4*5,"esp"));
	&add	($d_,&DWP(64+4*6,"esp"));
	&xor	($b_,&DWP(4*1,$b));
	&xor	($c, &DWP(4*2,$b));
	&xor	($c_,&DWP(4*3,$b));
	&xor	($d, &DWP(4*5,$b));
	&xor	($d_,&DWP(4*6,$b));
	&mov	(&DWP(4*1,$a),$b_);
	&mov	(&DWP(4*2,$a),$c);
	&mov	(&DWP(4*3,$a),$c_);
	&mov	(&DWP(4*5,$a),$d);
	&mov	(&DWP(4*6,$a),$d_);

	&mov	($b_,&DWP(4*7,"esp"));
	&mov	($c, &DWP(4*10,"esp"));
	&mov	($c_,&DWP(4*11,"esp"));
	&mov	($d, &DWP(4*13,"esp"));
	&mov	($d_,&DWP(4*15,"esp"));
	&add	($b_,&DWP(64+4*7,"esp"));
	&add	($c, &DWP(64+4*10,"esp"));
	&add	($c_,&DWP(64+4*11,"esp"));
	&add	($d, &DWP(64+4*13,"esp"));
	&add	($d_,&DWP(64+4*15,"esp"));
	&xor	($b_,&DWP(4*7,$b));
	&xor	($c, &DWP(4*10,$b));
	&xor	($c_,&DWP(4*11,$b));
	&xor	($d, &DWP(4*13,$b));
	&xor	($d_,&DWP(4*15,$b));
	&lea	($b,&DWP(4*16,$b));
	&mov	(&DWP(4*7,$a),$b_);
	&mov	($b_,&DWP(4*0,"esp"));
	&mov	(&DWP(4*10,$a),$c);
	&mov	($c,&wparam(2));		     
	&mov	(&DWP(4*11,$a),$c_);
	&mov	(&DWP(4*13,$a),$d);
	&mov	(&DWP(4*15,$a),$d_);
	&mov	(&DWP(4*0,$a),$b_);
	&lea	($a,&DWP(4*16,$a));
	&sub	($c,64);
	&jnz	(&label("outer_loop"));

	&jmp	(&label("done"));

&set_label("tail");
	&add	($d, &DWP(64+4*12,"esp"));
	&add	($d_,&DWP(64+4*14,"esp"));
	&mov	(&DWP(4*0,"esp"),$a);
	&mov	(&DWP(4*4,"esp"),$b_);
	&mov	(&DWP(4*8,"esp"),$c);
	&mov	(&DWP(4*9,"esp"),$c_);
	&mov	(&DWP(4*12,"esp"),$d);
	&mov	(&DWP(4*14,"esp"),$d_);

	&mov	($b_,&DWP(4*1,"esp"));
	&mov	($c, &DWP(4*2,"esp"));
	&mov	($c_,&DWP(4*3,"esp"));
	&mov	($d, &DWP(4*5,"esp"));
	&mov	($d_,&DWP(4*6,"esp"));
	&add	($b_,0x3320646e);		                         
	&add	($c, 0x79622d32);
	&add	($c_,0x6b206574);
	&add	($d, &DWP(64+4*5,"esp"));
	&add	($d_,&DWP(64+4*6,"esp"));
	&mov	(&DWP(4*1,"esp"),$b_);
	&mov	(&DWP(4*2,"esp"),$c);
	&mov	(&DWP(4*3,"esp"),$c_);
	&mov	(&DWP(4*5,"esp"),$d);
	&mov	(&DWP(4*6,"esp"),$d_);

	&mov	($b_,&DWP(4*7,"esp"));
	&mov	($c, &DWP(4*10,"esp"));
	&mov	($c_,&DWP(4*11,"esp"));
	&mov	($d, &DWP(4*13,"esp"));
	&mov	($d_,&DWP(4*15,"esp"));
	&add	($b_,&DWP(64+4*7,"esp"));
	&add	($c, &DWP(64+4*10,"esp"));
	&add	($c_,&DWP(64+4*11,"esp"));
	&add	($d, &DWP(64+4*13,"esp"));
	&add	($d_,&DWP(64+4*15,"esp"));
	&mov	(&DWP(4*7,"esp"),$b_);
	&mov	($b_,&wparam(1));		            
	&mov	(&DWP(4*10,"esp"),$c);
	&mov	($c,&wparam(0));		             
	&mov	(&DWP(4*11,"esp"),$c_);
	&xor	($c_,$c_);
	&mov	(&DWP(4*13,"esp"),$d);
	&mov	(&DWP(4*15,"esp"),$d_);

	&xor	("eax","eax");
	&xor	("edx","edx");
&set_label("tail_loop");
	&movb	("al",&BP(0,$c_,$b_));
	&movb	("dl",&BP(0,"esp",$c_));
	&lea	($c_,&DWP(1,$c_));
	&xor	("al","dl");
	&mov	(&BP(-1,$c,$c_),"al");
	&dec	($b);
	&jnz	(&label("tail_loop"));

&set_label("done");
	&stack_pop(33);
&function_end("ChaCha20_ctr32_nohw");

if ($xmm) {
my ($xa,$xa_,$xb,$xb_,$xc,$xc_,$xd,$xd_)=map("xmm$_",(0..7));
my ($out,$inp,$len)=("edi","esi","ecx");

sub QUARTERROUND_SSSE3 {
my ($ai,$bi,$ci,$di,$i)=@_;
my ($an,$bn,$cn,$dn)=map(($_&~3)+(($_+1)&3),($ai,$bi,$ci,$di));	      
my ($ap,$bp,$cp,$dp)=map(($_&~3)+(($_-1)&3),($ai,$bi,$ci,$di));	          

	                     
	 
	                                  
	                     
	                     
	                     
	                                 
	                     
	                     
	                     

	if ($i==0) {
            my $j=4;
	    ($ap,$bp,$cp,$dp)=map(($_&~3)+(($_-$j--)&3),($ap,$bp,$cp,$dp));
	} elsif ($i==3) {
            my $j=0;
	    ($an,$bn,$cn,$dn)=map(($_&~3)+(($_+$j++)&3),($an,$bn,$cn,$dn));
	} elsif ($i==4) {
            my $j=4;
	    ($ap,$bp,$cp,$dp)=map(($_&~3)+(($_+$j--)&3),($ap,$bp,$cp,$dp));
	} elsif ($i==7) {
            my $j=0;
	    ($an,$bn,$cn,$dn)=map(($_&~3)+(($_-$j++)&3),($an,$bn,$cn,$dn));
	}

	                                    
	                                   
	 &movdqa(&QWP(16*$cp-128,"ebx"),$xc_)	if ($ai>0 && $ai<3);
	&pshufb	($xd,&QWP(0,"eax"));		       
	 &movdqa(&QWP(16*$bp-128,"ebx"),$xb_)	if ($i!=0);
	&paddd	($xc,$xd);
	 &movdqa($xc_,&QWP(16*$cn-128,"ebx"))	if ($ai>0 && $ai<3);
	&pxor	($xb,$xc);
	 &movdqa($xb_,&QWP(16*$bn-128,"ebx"))	if ($i<7);
	&movdqa	($xa_,$xb);			                     
	&pslld	($xb,12);
	&psrld	($xa_,20);
	&por	($xb,$xa_);
	 &movdqa($xa_,&QWP(16*$an-128,"ebx"));
	&paddd	($xa,$xb);
	 &movdqa($xd_,&QWP(16*$dn-128,"ebx"))	if ($di!=$dn);
	&pxor	($xd,$xa);
	&movdqa	(&QWP(16*$ai-128,"ebx"),$xa);
	&pshufb	($xd,&QWP(16,"eax"));		      
	&paddd	($xc,$xd);
	&movdqa	(&QWP(16*$di-128,"ebx"),$xd)	if ($di!=$dn);
	&movdqa	($xd_,$xd)			if ($di==$dn);
	&pxor	($xb,$xc);
	 &paddd	($xa_,$xb_)			if ($i<7);	           
	&movdqa	($xa,$xb);			                     
	&pslld	($xb,7);
	&psrld	($xa,25);
	 &pxor	($xd_,$xa_)			if ($i<7);	           
	&por	($xb,$xa);

	($xa,$xa_)=($xa_,$xa);
	($xb,$xb_)=($xb_,$xb);
	($xc,$xc_)=($xc_,$xc);
	($xd,$xd_)=($xd_,$xd);
}

&function_begin("ChaCha20_ctr32_ssse3");
	&call	(&label("pic_point"));
&set_label("pic_point");
	&blindpop("eax");

	&mov		($out,&wparam(0));
	&mov		($inp,&wparam(1));
	&mov		($len,&wparam(2));
	&mov		("edx",&wparam(3));		     
	&mov		("ebx",&wparam(4));		                   

	&mov		("ebp","esp");
	&stack_push	(131);
	&and		("esp",-64);
	&mov		(&DWP(512,"esp"),"ebp");

	&lea		("eax",&DWP(&label("ssse3_data")."-".
				    &label("pic_point"),"eax"));
	&movdqu		("xmm3",&QWP(0,"ebx"));		                   

if (defined($gasver) && $gasver>=2.17) {		                       
							                     
							                      
							                      
							                     
							            
	&cmp		($len,64*4);
	&jb		(&label("1x"));

	&mov		(&DWP(512+4,"esp"),"edx");	                  
	&mov		(&DWP(512+8,"esp"),"ebx");
	&sub		($len,64*4);			          
	&lea		("ebp",&DWP(256+128,"esp"));	                   

	&movdqu		("xmm7",&QWP(0,"edx"));		     
	&pshufd		("xmm0","xmm3",0x00);
	&pshufd		("xmm1","xmm3",0x55);
	&pshufd		("xmm2","xmm3",0xaa);
	&pshufd		("xmm3","xmm3",0xff);
	 &paddd		("xmm0",&QWP(16*3,"eax"));	              
	&pshufd		("xmm4","xmm7",0x00);
	&pshufd		("xmm5","xmm7",0x55);
	 &psubd		("xmm0",&QWP(16*4,"eax"));
	&pshufd		("xmm6","xmm7",0xaa);
	&pshufd		("xmm7","xmm7",0xff);
	&movdqa		(&QWP(16*12-128,"ebp"),"xmm0");
	&movdqa		(&QWP(16*13-128,"ebp"),"xmm1");
	&movdqa		(&QWP(16*14-128,"ebp"),"xmm2");
	&movdqa		(&QWP(16*15-128,"ebp"),"xmm3");
	 &movdqu	("xmm3",&QWP(16,"edx"));	     
	&movdqa		(&QWP(16*4-128,"ebp"),"xmm4");
	&movdqa		(&QWP(16*5-128,"ebp"),"xmm5");
	&movdqa		(&QWP(16*6-128,"ebp"),"xmm6");
	&movdqa		(&QWP(16*7-128,"ebp"),"xmm7");
	 &movdqa	("xmm7",&QWP(16*2,"eax"));	       
	 &lea		("ebx",&DWP(128,"esp"));	                   

	&pshufd		("xmm0","xmm3",0x00);
	&pshufd		("xmm1","xmm3",0x55);
	&pshufd		("xmm2","xmm3",0xaa);
	&pshufd		("xmm3","xmm3",0xff);
	&pshufd		("xmm4","xmm7",0x00);
	&pshufd		("xmm5","xmm7",0x55);
	&pshufd		("xmm6","xmm7",0xaa);
	&pshufd		("xmm7","xmm7",0xff);
	&movdqa		(&QWP(16*8-128,"ebp"),"xmm0");
	&movdqa		(&QWP(16*9-128,"ebp"),"xmm1");
	&movdqa		(&QWP(16*10-128,"ebp"),"xmm2");
	&movdqa		(&QWP(16*11-128,"ebp"),"xmm3");
	&movdqa		(&QWP(16*0-128,"ebp"),"xmm4");
	&movdqa		(&QWP(16*1-128,"ebp"),"xmm5");
	&movdqa		(&QWP(16*2-128,"ebp"),"xmm6");
	&movdqa		(&QWP(16*3-128,"ebp"),"xmm7");

	&lea		($inp,&DWP(128,$inp));		                   
	&lea		($out,&DWP(128,$out));		                   
	&jmp		(&label("outer_loop"));

&set_label("outer_loop",16);
	                                                           
	&movdqa		("xmm1",&QWP(16*1-128,"ebp"));
	&movdqa		("xmm2",&QWP(16*2-128,"ebp"));
	&movdqa		("xmm3",&QWP(16*3-128,"ebp"));
	                                       
	&movdqa		("xmm5",&QWP(16*5-128,"ebp"));
	&movdqa		("xmm6",&QWP(16*6-128,"ebp"));
	&movdqa		("xmm7",&QWP(16*7-128,"ebp"));
	                                       
	&movdqa		(&QWP(16*1-128,"ebx"),"xmm1");
	&movdqa		(&QWP(16*2-128,"ebx"),"xmm2");
	&movdqa		(&QWP(16*3-128,"ebx"),"xmm3");
	                                       
	&movdqa		(&QWP(16*5-128,"ebx"),"xmm5");
	&movdqa		(&QWP(16*6-128,"ebx"),"xmm6");
	&movdqa		(&QWP(16*7-128,"ebx"),"xmm7");
	                                       
	                                       
	&movdqa		("xmm2",&QWP(16*10-128,"ebp"));
	&movdqa		("xmm3",&QWP(16*11-128,"ebp"));
	&movdqa		("xmm4",&QWP(16*12-128,"ebp"));
	&movdqa		("xmm5",&QWP(16*13-128,"ebp"));
	&movdqa		("xmm6",&QWP(16*14-128,"ebp"));
	&movdqa		("xmm7",&QWP(16*15-128,"ebp"));
	&paddd		("xmm4",&QWP(16*4,"eax"));	               
	                                       
	                                       
	&movdqa		(&QWP(16*10-128,"ebx"),"xmm2");
	&movdqa		(&QWP(16*11-128,"ebx"),"xmm3");
	&movdqa		(&QWP(16*12-128,"ebx"),"xmm4");
	&movdqa		(&QWP(16*13-128,"ebx"),"xmm5");
	&movdqa		(&QWP(16*14-128,"ebx"),"xmm6");
	&movdqa		(&QWP(16*15-128,"ebx"),"xmm7");
	&movdqa		(&QWP(16*12-128,"ebp"),"xmm4");	                    

	&movdqa		($xa, &QWP(16*0-128,"ebp"));
	&movdqa		($xd, "xmm4");
	&movdqa		($xb_,&QWP(16*4-128,"ebp"));
	&movdqa		($xc, &QWP(16*8-128,"ebp"));
	&movdqa		($xc_,&QWP(16*9-128,"ebp"));

	&mov		("edx",10);			              
	&nop		();

&set_label("loop",16);
	&paddd		($xa,$xb_);			           
	&movdqa		($xb,$xb_);
	&pxor		($xd,$xa);			           
	&QUARTERROUND_SSSE3(0, 4, 8, 12, 0);
	&QUARTERROUND_SSSE3(1, 5, 9, 13, 1);
	&QUARTERROUND_SSSE3(2, 6,10, 14, 2);
	&QUARTERROUND_SSSE3(3, 7,11, 15, 3);
	&QUARTERROUND_SSSE3(0, 5,10, 15, 4);
	&QUARTERROUND_SSSE3(1, 6,11, 12, 5);
	&QUARTERROUND_SSSE3(2, 7, 8, 13, 6);
	&QUARTERROUND_SSSE3(3, 4, 9, 14, 7);
	&dec		("edx");
	&jnz		(&label("loop"));

	&movdqa		(&QWP(16*4-128,"ebx"),$xb_);
	&movdqa		(&QWP(16*8-128,"ebx"),$xc);
	&movdqa		(&QWP(16*9-128,"ebx"),$xc_);
	&movdqa		(&QWP(16*12-128,"ebx"),$xd);
	&movdqa		(&QWP(16*14-128,"ebx"),$xd_);

    my ($xa0,$xa1,$xa2,$xa3,$xt0,$xt1,$xt2,$xt3)=map("xmm$_",(0..7));

	                                                  
	&movdqa		($xa1,&QWP(16*1-128,"ebx"));
	&movdqa		($xa2,&QWP(16*2-128,"ebx"));
	&movdqa		($xa3,&QWP(16*3-128,"ebx"));

    for($i=0;$i<256;$i+=64) {
	&paddd		($xa0,&QWP($i+16*0-128,"ebp"));	                         
	&paddd		($xa1,&QWP($i+16*1-128,"ebp"));
	&paddd		($xa2,&QWP($i+16*2-128,"ebp"));
	&paddd		($xa3,&QWP($i+16*3-128,"ebp"));

	&movdqa		($xt2,$xa0);		                     
	&punpckldq	($xa0,$xa1);
	&movdqa		($xt3,$xa2);
	&punpckldq	($xa2,$xa3);
	&punpckhdq	($xt2,$xa1);
	&punpckhdq	($xt3,$xa3);
	&movdqa		($xa1,$xa0);
	&punpcklqdq	($xa0,$xa2);		      
	&movdqa		($xa3,$xt2);
	&punpcklqdq	($xt2,$xt3);		      
	&punpckhqdq	($xa1,$xa2);		      
	&punpckhqdq	($xa3,$xt3);		      

	                         

	&movdqu		($xt0,&QWP(64*0-128,$inp));	            
	&movdqu		($xt1,&QWP(64*1-128,$inp));
	&movdqu		($xa2,&QWP(64*2-128,$inp));
	&movdqu		($xt3,&QWP(64*3-128,$inp));
	&lea		($inp,&QWP($i<192?16:(64*4-16*3),$inp));
	&pxor		($xt0,$xa0);
	&movdqa		($xa0,&QWP($i+16*4-128,"ebx"))	if ($i<192);
	&pxor		($xt1,$xa1);
	&movdqa		($xa1,&QWP($i+16*5-128,"ebx"))	if ($i<192);
	&pxor		($xt2,$xa2);
	&movdqa		($xa2,&QWP($i+16*6-128,"ebx"))	if ($i<192);
	&pxor		($xt3,$xa3);
	&movdqa		($xa3,&QWP($i+16*7-128,"ebx"))	if ($i<192);
	&movdqu		(&QWP(64*0-128,$out),$xt0);	              
	&movdqu		(&QWP(64*1-128,$out),$xt1);
	&movdqu		(&QWP(64*2-128,$out),$xt2);
	&movdqu		(&QWP(64*3-128,$out),$xt3);
	&lea		($out,&QWP($i<192?16:(64*4-16*3),$out));
    }
	&sub		($len,64*4);
	&jnc		(&label("outer_loop"));

	&add		($len,64*4);
	&jz		(&label("done"));

	&mov		("ebx",&DWP(512+8,"esp"));	                  
	&lea		($inp,&DWP(-128,$inp));
	&mov		("edx",&DWP(512+4,"esp"));
	&lea		($out,&DWP(-128,$out));

	&movd		("xmm2",&DWP(16*12-128,"ebp"));	               
	&movdqu		("xmm3",&QWP(0,"ebx"));
	&paddd		("xmm2",&QWP(16*6,"eax"));	       
	&pand		("xmm3",&QWP(16*7,"eax"));
	&por		("xmm3","xmm2");		               
}
{
my ($a,$b,$c,$d,$t,$t1,$rot16,$rot24)=map("xmm$_",(0..7));

sub SSSE3ROUND {	                                            
	&paddd		($a,$b);
	&pxor		($d,$a);
	&pshufb		($d,$rot16);

	&paddd		($c,$d);
	&pxor		($b,$c);
	&movdqa		($t,$b);
	&psrld		($b,20);
	&pslld		($t,12);
	&por		($b,$t);

	&paddd		($a,$b);
	&pxor		($d,$a);
	&pshufb		($d,$rot24);

	&paddd		($c,$d);
	&pxor		($b,$c);
	&movdqa		($t,$b);
	&psrld		($b,25);
	&pslld		($t,7);
	&por		($b,$t);
}

&set_label("1x");
	&movdqa		($a,&QWP(16*2,"eax"));		       
	&movdqu		($b,&QWP(0,"edx"));
	&movdqu		($c,&QWP(16,"edx"));
	                                              
	&movdqa		($rot16,&QWP(0,"eax"));
	&movdqa		($rot24,&QWP(16,"eax"));
	&mov		(&DWP(16*3,"esp"),"ebp");

	&movdqa		(&QWP(16*0,"esp"),$a);
	&movdqa		(&QWP(16*1,"esp"),$b);
	&movdqa		(&QWP(16*2,"esp"),$c);
	&movdqa		(&QWP(16*3,"esp"),$d);
	&mov		("edx",10);
	&jmp		(&label("loop1x"));

&set_label("outer1x",16);
	&movdqa		($d,&QWP(16*5,"eax"));		     
	&movdqa		($a,&QWP(16*0,"esp"));
	&movdqa		($b,&QWP(16*1,"esp"));
	&movdqa		($c,&QWP(16*2,"esp"));
	&paddd		($d,&QWP(16*3,"esp"));
	&mov		("edx",10);
	&movdqa		(&QWP(16*3,"esp"),$d);
	&jmp		(&label("loop1x"));

&set_label("loop1x",16);
	&SSSE3ROUND();
	&pshufd	($c,$c,0b01001110);
	&pshufd	($b,$b,0b00111001);
	&pshufd	($d,$d,0b10010011);
	&nop	();

	&SSSE3ROUND();
	&pshufd	($c,$c,0b01001110);
	&pshufd	($b,$b,0b10010011);
	&pshufd	($d,$d,0b00111001);

	&dec		("edx");
	&jnz		(&label("loop1x"));

	&paddd		($a,&QWP(16*0,"esp"));
	&paddd		($b,&QWP(16*1,"esp"));
	&paddd		($c,&QWP(16*2,"esp"));
	&paddd		($d,&QWP(16*3,"esp"));

	&cmp		($len,64);
	&jb		(&label("tail"));

	&movdqu		($t,&QWP(16*0,$inp));
	&movdqu		($t1,&QWP(16*1,$inp));
	&pxor		($a,$t);		                
	&movdqu		($t,&QWP(16*2,$inp));
	&pxor		($b,$t1);
	&movdqu		($t1,&QWP(16*3,$inp));
	&pxor		($c,$t);
	&pxor		($d,$t1);
	&lea		($inp,&DWP(16*4,$inp));	         

	&movdqu		(&QWP(16*0,$out),$a);	              
	&movdqu		(&QWP(16*1,$out),$b);
	&movdqu		(&QWP(16*2,$out),$c);
	&movdqu		(&QWP(16*3,$out),$d);
	&lea		($out,&DWP(16*4,$out));	         

	&sub		($len,64);
	&jnz		(&label("outer1x"));

	&jmp		(&label("done"));

&set_label("tail");
	&movdqa		(&QWP(16*0,"esp"),$a);
	&movdqa		(&QWP(16*1,"esp"),$b);
	&movdqa		(&QWP(16*2,"esp"),$c);
	&movdqa		(&QWP(16*3,"esp"),$d);

	&xor		("eax","eax");
	&xor		("edx","edx");
	&xor		("ebp","ebp");

&set_label("tail_loop");
	&movb		("al",&BP(0,"esp","ebp"));
	&movb		("dl",&BP(0,$inp,"ebp"));
	&lea		("ebp",&DWP(1,"ebp"));
	&xor		("al","dl");
	&movb		(&BP(-1,$out,"ebp"),"al");
	&dec		($len);
	&jnz		(&label("tail_loop"));
}
&set_label("done");
	&mov		("esp",&DWP(512,"esp"));
&function_end("ChaCha20_ctr32_ssse3");

&align	(64);
&set_label("ssse3_data");
&data_byte(0x2,0x3,0x0,0x1, 0x6,0x7,0x4,0x5, 0xa,0xb,0x8,0x9, 0xe,0xf,0xc,0xd);
&data_byte(0x3,0x0,0x1,0x2, 0x7,0x4,0x5,0x6, 0xb,0x8,0x9,0xa, 0xf,0xc,0xd,0xe);
&data_word(0x61707865,0x3320646e,0x79622d32,0x6b206574);
&data_word(0,1,2,3);
&data_word(4,4,4,4);
&data_word(1,0,0,0);
&data_word(4,0,0,0);
&data_word(0,-1,-1,-1);
&align	(64);
}
&asciz	("ChaCha20 for x86, CRYPTOGAMS by <appro\@openssl.org>");

&asm_finish();

close STDOUT or die "error closing STDOUT: $!";
