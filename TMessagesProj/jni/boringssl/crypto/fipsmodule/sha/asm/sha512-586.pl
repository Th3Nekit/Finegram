#! /usr/bin/env perl
# Copyright 2007-2016 The OpenSSL Project Authors. All Rights Reserved.
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
# SHA512 block transform for x86. September 2007.
#
# May 2013.
#
# Add SSSE3 code path, 20-25% improvement [over original SSE2 code].
#
# Performance in clock cycles per processed byte (less is better):
#
#		gcc	icc	x86 asm	SIMD(*)	x86_64(**)
# Pentium	100	97	61	-	-
# PIII		75	77	56	-	-
# P4		116	95	82	34.6	30.8
# AMD K8	54	55	36	20.7	9.57
# Core2		66	57	40	15.9	9.97
# Westmere	70	-	38	12.2	9.58
# Sandy Bridge	58	-	35	11.9	11.2
# Ivy Bridge	50	-	33	11.5	8.17
# Haswell	46	-	29	11.3	7.66
# Skylake	40	-	26	13.3	7.25
# Bulldozer	121	-	50	14.0	13.5
# VIA Nano	91	-	52	33	14.7
# Atom		126	-	68	48(***)	14.7
# Silvermont	97	-	58	42(***)	17.5
# Goldmont	80	-	48	19.5	12.0
#
# (*)	whichever best applicable.
# (**)	x86_64 assembler performance is presented for reference
#	purposes, the results are for integer-only code.
# (***)	paddq is incredibly slow on Atom.
#
# IALU code-path is optimized for elder Pentiums. On vanilla Pentium
# performance improvement over compiler generated code reaches ~60%,
# while on PIII - ~35%. On newer µ-archs improvement varies from 15%
# to 50%, but it's less important as they are expected to execute SSE2
# code-path, which is commonly ~2-3x faster [than compiler generated
# code]. SSE2 code-path is as fast as original sha512-sse2.pl, even
# though it does not use 128-bit operations. The latter means that
# SSE2-aware kernel is no longer required to execute the code. Another
# difference is that new code optimizes amount of writes, but at the
# cost of increased data cache "footprint" by 1/2KB.

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC,"${dir}","${dir}../../../perlasm");
require "x86asm.pl";

$output=pop;
open STDOUT,">$output";

&asm_init($ARGV[0]);

$sse2=1;

$Tlo=&DWP(0,"esp");	$Thi=&DWP(4,"esp");
$Alo=&DWP(8,"esp");	$Ahi=&DWP(8+4,"esp");
$Blo=&DWP(16,"esp");	$Bhi=&DWP(16+4,"esp");
$Clo=&DWP(24,"esp");	$Chi=&DWP(24+4,"esp");
$Dlo=&DWP(32,"esp");	$Dhi=&DWP(32+4,"esp");
$Elo=&DWP(40,"esp");	$Ehi=&DWP(40+4,"esp");
$Flo=&DWP(48,"esp");	$Fhi=&DWP(48+4,"esp");
$Glo=&DWP(56,"esp");	$Ghi=&DWP(56+4,"esp");
$Hlo=&DWP(64,"esp");	$Hhi=&DWP(64+4,"esp");
$K512="ebp";

$Asse2=&QWP(0,"esp");
$Bsse2=&QWP(8,"esp");
$Csse2=&QWP(16,"esp");
$Dsse2=&QWP(24,"esp");
$Esse2=&QWP(32,"esp");
$Fsse2=&QWP(40,"esp");
$Gsse2=&QWP(48,"esp");
$Hsse2=&QWP(56,"esp");

$A="mm0";	         
$E="mm4";	                                                     
		                                              
$BxC="mm2";	                    

sub BODY_00_15_sse2 {
    my $phase=shift;

	                                 
	                                 

	&movq	("mm1",$E);			                       
	 &pxor	("mm5","mm6");			      
	&psrlq	("mm1",14);
	 &movq	($Esse2,$E);			                         
	 &pand	("mm5",$E);			      
	&psllq	($E,23);			                    
	 &movq	($A,"mm3")			if ($phase<2);
	 &movq	(&QWP(8*9,"esp"),"mm7")		           
	&movq	("mm3","mm1");			            
	 &psrlq	("mm1",4);
	 &pxor	("mm5","mm6");			           
	&pxor	("mm3",$E);
	 &psllq	($E,23);
	&pxor	("mm3","mm1");
	 &movq	($Asse2,$A);			                         
	 &paddq	("mm7","mm5");			                 
	&pxor	("mm3",$E);
	 &psrlq	("mm1",23);
	 &paddq	("mm7",$Hsse2);			         
	&pxor	("mm3","mm1");
	 &psllq	($E,4);
	 &paddq	("mm7",QWP(0,$K512));		               
	&pxor	("mm3",$E);			                  

	 &movq	($E,$Dsse2);			                             
	&paddq	("mm3","mm7");			          
	 &movq	("mm5",$A);			                       
	 &psrlq	("mm5",28);
	&paddq	($E,"mm3");			         
	 &movq	("mm6",$A);			                      
	 &movq	("mm7","mm5");
	 &psllq	("mm6",25);
	&movq	("mm1",$Bsse2);			        
	 &psrlq	("mm5",6);
	 &pxor	("mm7","mm6");
	&sub	("esp",8);
	 &psllq	("mm6",5);
	 &pxor	("mm7","mm5");
	&pxor	($A,"mm1");			                        
	 &psrlq	("mm5",5);
	 &pxor	("mm7","mm6");
	&pand	($BxC,$A);			             
	 &psllq	("mm6",6);
	 &pxor	("mm7","mm5");
	&pxor	($BxC,"mm1");			                
	 &pxor	("mm6","mm7");			               
	 &movq	("mm7",&QWP(8*(9+16-1),"esp"))	if ($phase!=0);	           
	 &movq	("mm5",$Fsse2)			if ($phase==0);	        

    if ($phase>1) {
	&paddq	($BxC,"mm6");			              
	 &add	($K512,8);
	                               

	($A,$BxC) = ($BxC,$A);			                  
    } else {
	&paddq	("mm3",$BxC);			                
	 &movq	($BxC,$A);
	 &add	($K512,8);
	&paddq	("mm3","mm6");			               
	 &movq	("mm6",$Gsse2)			if ($phase==0);	        
	                           
    }
}

sub BODY_00_15_x86 {
	                                                               
	                                                  
	                                                  
	&mov	("ecx",$Elo);
	&mov	("edx",$Ehi);
	&mov	("esi","ecx");

	&shr	("ecx",9);	       
	&mov	("edi","edx");
	&shr	("edx",9);	       
	&mov	("ebx","ecx");
	&shl	("esi",14);	        
	&mov	("eax","edx");
	&shl	("edi",14);	        
	&xor	("ebx","esi");

	&shr	("ecx",14-9);	        
	&xor	("eax","edi");
	&shr	("edx",14-9);	        
	&xor	("eax","ecx");
	&shl	("esi",18-14);	        
	&xor	("ebx","edx");
	&shl	("edi",18-14);	        
	&xor	("ebx","esi");

	&shr	("ecx",18-14);	        
	&xor	("eax","edi");
	&shr	("edx",18-14);	        
	&xor	("eax","ecx");
	&shl	("esi",23-18);	        
	&xor	("ebx","edx");
	&shl	("edi",23-18);	        
	&xor	("eax","esi");
	&xor	("ebx","edi");			                

	&mov	("ecx",$Flo);
	&mov	("edx",$Fhi);
	&mov	("esi",$Glo);
	&mov	("edi",$Ghi);
	 &add	("eax",$Hlo);
	 &adc	("ebx",$Hhi);			         
	&xor	("ecx","esi");
	&xor	("edx","edi");
	&and	("ecx",$Elo);
	&and	("edx",$Ehi);
	 &add	("eax",&DWP(8*(9+15)+0,"esp"));
	 &adc	("ebx",&DWP(8*(9+15)+4,"esp"));	            
	&xor	("ecx","esi");
	&xor	("edx","edi");			                        

	&mov	("esi",&DWP(0,$K512));
	&mov	("edi",&DWP(4,$K512));		      
	&add	("eax","ecx");
	&adc	("ebx","edx");			                 
	&mov	("ecx",$Dlo);
	&mov	("edx",$Dhi);
	&add	("eax","esi");
	&adc	("ebx","edi");			            
	&mov	($Tlo,"eax");
	&mov	($Thi,"ebx");			             
	&add	("eax","ecx");
	&adc	("ebx","edx");			         

	                                                              
	                                                 
	                                                 
	&mov	("ecx",$Alo);
	&mov	("edx",$Ahi);
	&mov	($Dlo,"eax");
	&mov	($Dhi,"ebx");
	&mov	("esi","ecx");

	&shr	("ecx",2);	       
	&mov	("edi","edx");
	&shr	("edx",2);	       
	&mov	("ebx","ecx");
	&shl	("esi",4);	       
	&mov	("eax","edx");
	&shl	("edi",4);	       
	&xor	("ebx","esi");

	&shr	("ecx",7-2);	       
	&xor	("eax","edi");
	&shr	("edx",7-2);	       
	&xor	("ebx","ecx");
	&shl	("esi",25-4);	        
	&xor	("eax","edx");
	&shl	("edi",25-4);	        
	&xor	("eax","esi");

	&shr	("ecx",28-7);	        
	&xor	("ebx","edi");
	&shr	("edx",28-7);	        
	&xor	("eax","ecx");
	&shl	("esi",30-25);	        
	&xor	("ebx","edx");
	&shl	("edi",30-25);	        
	&xor	("eax","esi");
	&xor	("ebx","edi");			           

	&mov	("ecx",$Alo);
	&mov	("edx",$Ahi);
	&mov	("esi",$Blo);
	&mov	("edi",$Bhi);
	&add	("eax",$Tlo);
	&adc	("ebx",$Thi);			                   
	&or	("ecx","esi");
	&or	("edx","edi");
	&and	("ecx",$Clo);
	&and	("edx",$Chi);
	&and	("esi",$Alo);
	&and	("edi",$Ahi);
	&or	("ecx","esi");
	&or	("edx","edi");			                              

	&add	("eax","ecx");
	&adc	("ebx","edx");			                  
	&mov	($Tlo,"eax");
	&mov	($Thi,"ebx");

	&mov	(&LB("edx"),&BP(0,$K512));	                     
	&sub	("esp",8);
	&lea	($K512,&DWP(8,$K512));		     
}

&static_label("K512");

&function_begin("sha512_block_data_order_nohw");
	&mov	("esi",wparam(0));	     
	&mov	("edi",wparam(1));	     
	&mov	("eax",wparam(2));	     
	&mov	("ebx","esp");		          

	&call	(&label("pic_point"));	              
&set_label("pic_point");
	&blindpop($K512);
	&lea	($K512,&DWP(&label("K512")."-".&label("pic_point"),$K512));

	&sub	("esp",16);
	&and	("esp",-64);

	&shl	("eax",7);
	&add	("eax","edi");
	&mov	(&DWP(0,"esp"),"esi");	     
	&mov	(&DWP(4,"esp"),"edi");	     
	&mov	(&DWP(8,"esp"),"eax");	             
	&mov	(&DWP(12,"esp"),"ebx");	          

if ($sse2) {
	                  
	&movq	($A,&QWP(0,"esi"));
	&movq	("mm1",&QWP(8,"esi"));
	&movq	($BxC,&QWP(16,"esi"));
	&movq	("mm3",&QWP(24,"esi"));
	&movq	($E,&QWP(32,"esi"));
	&movq	("mm5",&QWP(40,"esi"));
	&movq	("mm6",&QWP(48,"esi"));
	&movq	("mm7",&QWP(56,"esi"));
	&sub	("esp",8*10);
	&jmp	(&label("loop_sse2"));

	                                                                       
	                                                                      
	                                                                     
	                                                                 
	          

&set_label("loop_sse2",16);
	                   
	&movq	($Bsse2,"mm1");
	&movq	($Csse2,$BxC);
	&movq	($Dsse2,"mm3");
	                   
	&movq	($Fsse2,"mm5");
	&movq	($Gsse2,"mm6");
	&pxor	($BxC,"mm1");			       
	&movq	($Hsse2,"mm7");
	&movq	("mm3",$A);			       

	&mov	("eax",&DWP(0,"edi"));
	&mov	("ebx",&DWP(4,"edi"));
	&add	("edi",8);
	&mov	("edx",15);			         
	&bswap	("eax");
	&bswap	("ebx");
	&jmp	(&label("00_14_sse2"));

&set_label("00_14_sse2",16);
	&movd	("mm1","eax");
	&mov	("eax",&DWP(0,"edi"));
	&movd	("mm7","ebx");
	&mov	("ebx",&DWP(4,"edi"));
	&add	("edi",8);
	&bswap	("eax");
	&bswap	("ebx");
	&punpckldq("mm7","mm1");

	&BODY_00_15_sse2();

	&dec	("edx");
	&jnz	(&label("00_14_sse2"));

	&movd	("mm1","eax");
	&movd	("mm7","ebx");
	&punpckldq("mm7","mm1");

	&BODY_00_15_sse2(1);

	&pxor	($A,$A);			              
	&mov	("edx",32);			         
	&jmp	(&label("16_79_sse2"));

&set_label("16_79_sse2",16);
    for ($j=0;$j<2;$j++) {			           
	                                                                 
	&movq	("mm5",&QWP(8*(9+16-14),"esp"));
	&movq	("mm1","mm7");
	&psrlq	("mm7",1);
	 &movq	("mm6","mm5");
	 &psrlq	("mm5",6);
	&psllq	("mm1",56);
	 &paddq	($A,"mm3");			                 
	 &movq	("mm3","mm7");
	&psrlq	("mm7",7-1);
	 &pxor	("mm3","mm1");
	 &psllq	("mm1",63-56);
	&pxor	("mm3","mm7");
	 &psrlq	("mm7",8-7);
	&pxor	("mm3","mm1");
	 &movq	("mm1","mm5");
	 &psrlq	("mm5",19-6);
	&pxor	("mm7","mm3");			        

	 &psllq	("mm6",3);
	 &pxor	("mm1","mm5");
	&paddq	("mm7",&QWP(8*(9+16),"esp"));
	 &pxor	("mm1","mm6");
	 &psrlq	("mm5",61-19);
	&paddq	("mm7",&QWP(8*(9+16-9),"esp"));
	 &pxor	("mm1","mm5");
	 &psllq	("mm6",45-3);
	&movq	("mm5",$Fsse2);			        
	 &pxor	("mm1","mm6");			        
	&movq	("mm6",$Gsse2);			        

	&paddq	("mm7","mm1");			      
	                                                     

	&BODY_00_15_sse2(2);
    }
	&dec	("edx");
	&jnz	(&label("16_79_sse2"));

	                   
	&paddq	($A,"mm3");			                 
	&movq	("mm1",$Bsse2);
	                     
	&movq	("mm3",$Dsse2);
	                   
	&movq	("mm5",$Fsse2);
	&movq	("mm6",$Gsse2);
	&movq	("mm7",$Hsse2);

	&pxor	($BxC,"mm1");			          
	&paddq	($A,&QWP(0,"esi"));
	&paddq	("mm1",&QWP(8,"esi"));
	&paddq	($BxC,&QWP(16,"esi"));
	&paddq	("mm3",&QWP(24,"esi"));
	&paddq	($E,&QWP(32,"esi"));
	&paddq	("mm5",&QWP(40,"esi"));
	&paddq	("mm6",&QWP(48,"esi"));
	&paddq	("mm7",&QWP(56,"esi"));

	&mov	("eax",8*80);
	&movq	(&QWP(0,"esi"),$A);
	&movq	(&QWP(8,"esi"),"mm1");
	&movq	(&QWP(16,"esi"),$BxC);
	&movq	(&QWP(24,"esi"),"mm3");
	&movq	(&QWP(32,"esi"),$E);
	&movq	(&QWP(40,"esi"),"mm5");
	&movq	(&QWP(48,"esi"),"mm6");
	&movq	(&QWP(56,"esi"),"mm7");

	&lea	("esp",&DWP(0,"esp","eax"));	               
	&sub	($K512,"eax");			          

	&cmp	("edi",&DWP(8*10+8,"esp"));	                  
	&jb	(&label("loop_sse2"));

	&mov	("esp",&DWP(8*10+12,"esp"));	            
	&emms	();
&function_end("sha512_block_data_order_nohw");

{ my ($cnt,$frame)=("ecx","edx");
  my @X=map("xmm$_",(0..7));
  my $j;
  my $i=0;

&function_begin("sha512_block_data_order_ssse3");
	&mov	("esi",wparam(0));	     
	&mov	("edi",wparam(1));	     
	&mov	("eax",wparam(2));	     
	&mov	("ebx","esp");		          

	&call	(&label("pic_point"));	              
&set_label("pic_point");
	&blindpop($K512);
	&lea	($K512,&DWP(&label("K512")."-".&label("pic_point"),$K512));

	&sub	("esp",16);
	&and	("esp",-64);

	&shl	("eax",7);
	&add	("eax","edi");
	&mov	(&DWP(0,"esp"),"esi");	     
	&mov	(&DWP(4,"esp"),"edi");	     
	&mov	(&DWP(8,"esp"),"eax");	             
	&mov	(&DWP(12,"esp"),"ebx");	          

	                  
	&movq	($A,&QWP(0,"esi"));
	&movq	("mm1",&QWP(8,"esi"));
	&movq	($BxC,&QWP(16,"esi"));
	&movq	("mm3",&QWP(24,"esi"));
	&movq	($E,&QWP(32,"esi"));
	&movq	("mm5",&QWP(40,"esi"));
	&movq	("mm6",&QWP(48,"esi"));
	&movq	("mm7",&QWP(56,"esi"));

	                                                                       
	                                                                      
	                                                                     
	                                                                 
	          

	&lea	($frame,&DWP(-64,"esp"));
	&sub	("esp",256);

	                          
	 
	                                     
	                                                 
	                                    
	                            

	&movdqa		(@X[1],&QWP(80*8,$K512));		                
	&movdqu		(@X[0],&QWP(0,"edi"));
	&pshufb		(@X[0],@X[1]);
    for ($j=0;$j<8;$j++) {
	&movdqa		(&QWP(16*(($j-1)%4),$frame),@X[3])	if ($j>4);           
	&movdqa		(@X[3],&QWP(16*($j%8),$K512));
	&movdqa		(@X[2],@X[1])				if ($j<7);                            
	&movdqu		(@X[1],&QWP(16*($j+1),"edi"))		if ($j<7);             
	&movdqa		(@X[1],&QWP(16*(($j+1)%4),$frame))	if ($j==7);               
	&paddq		(@X[3],@X[0]);
	&pshufb		(@X[1],@X[2])				if ($j<7);
	&movdqa		(&QWP(16*($j%8)-128,$frame),@X[3]);	                

	push(@X,shift(@X));					            
    }
	                              
	&nop		();

&set_label("loop_ssse3",32);
	&movdqa		(@X[2],&QWP(16*(($j+1)%4),$frame));	                   
	&movdqa		(&QWP(16*(($j-1)%4),$frame),@X[3]);	                
	&lea		($K512,&DWP(16*8,$K512));

	                                    
	&movq	($Bsse2,"mm1");
	 &mov	("ebx","edi");
	&movq	($Csse2,$BxC);
	 &lea	("edi",&DWP(128,"edi"));	               
	&movq	($Dsse2,"mm3");
	 &cmp	("edi","eax");
	                   
	&movq	($Fsse2,"mm5");
	 &cmovb	("ebx","edi");
	&movq	($Gsse2,"mm6");
	 &mov	("ecx",4);			              
	&pxor	($BxC,"mm1");			       
	&movq	($Hsse2,"mm7");
	&pxor	("mm3","mm3");			       

	&jmp		(&label("00_47_ssse3"));

sub BODY_00_15_ssse3 {		                                      
	(
	'&movq	("mm1",$E)',				                       
	'&movq	("mm7",&QWP(((-8*$i)%128)-128,$frame))',           
	 '&pxor	("mm5","mm6")',				      
	'&psrlq	("mm1",14)',
	 '&movq	(&QWP(8*($i+4)%64,"esp"),$E)',		                         
	 '&pand	("mm5",$E)',				      
	'&psllq	($E,23)',				                    
	'&paddq	($A,"mm3")',				                 
	'&movq	("mm3","mm1")',				            
	 '&psrlq("mm1",4)',
	 '&pxor	("mm5","mm6")',				           
	'&pxor	("mm3",$E)',
	 '&psllq($E,23)',
	'&pxor	("mm3","mm1")',
	 '&movq	(&QWP(8*$i%64,"esp"),$A)',		                         
	 '&paddq("mm7","mm5")',				                 
	'&pxor	("mm3",$E)',
	 '&psrlq("mm1",23)',
	 '&paddq("mm7",&QWP(8*($i+7)%64,"esp"))',	         
	'&pxor	("mm3","mm1")',
	 '&psllq($E,4)',
	'&pxor	("mm3",$E)',				                  

	 '&movq	($E,&QWP(8*($i+3)%64,"esp"))',		                             
	'&paddq	("mm3","mm7")',				          
	 '&movq	("mm5",$A)',				                       
	 '&psrlq("mm5",28)',
	'&paddq	($E,"mm3")',				         
	 '&movq	("mm6",$A)',				                      
	 '&movq	("mm7","mm5")',
	 '&psllq("mm6",25)',
	'&movq	("mm1",&QWP(8*($i+1)%64,"esp"))',	        
	 '&psrlq("mm5",6)',
	 '&pxor	("mm7","mm6")',
	 '&psllq("mm6",5)',
	 '&pxor	("mm7","mm5")',
	'&pxor	($A,"mm1")',				                        
	 '&psrlq("mm5",5)',
	 '&pxor	("mm7","mm6")',
	'&pand	($BxC,$A)',				             
	 '&psllq("mm6",6)',
	 '&pxor	("mm7","mm5")',
	'&pxor	($BxC,"mm1")',				                
	 '&pxor	("mm6","mm7")',				               
	 '&movq	("mm5",&QWP(8*($i+5-1)%64,"esp"))',	            
	'&paddq	($BxC,"mm6")',				              
	 '&movq	("mm6",&QWP(8*($i+6-1)%64,"esp"))',	            

	'($A,$BxC) = ($BxC,$A); $i--;'
	);
}

&set_label("00_47_ssse3",32);

    for(;$j<16;$j++) {
	my ($t0,$t2,$t1)=@X[2..4];
	my @insns = (&BODY_00_15_ssse3(),&BODY_00_15_ssse3());

	&movdqa		($t2,@X[5]);
	&movdqa		(@X[1],$t0);			               
	&palignr	($t0,@X[0],8);			         
	&movdqa		(&QWP(16*($j%4),$frame),@X[4]);	                
	 &palignr	($t2,@X[4],8);			          

	&movdqa		($t1,$t0);
	&psrlq		($t0,7);
	 &paddq		(@X[0],$t2);			                     
	&movdqa		($t2,$t1);
	&psrlq		($t1,1);
	&psllq		($t2,64-8);
	&pxor		($t0,$t1);
	&psrlq		($t1,8-1);
	&pxor		($t0,$t2);
	&psllq		($t2,8-1);
	&pxor		($t0,$t1);
	 &movdqa	($t1,@X[7]);
	&pxor		($t0,$t2);			                 
	 &movdqa	($t2,@X[7]);
	 &psrlq		($t1,6);
	&paddq		(@X[0],$t0);			                            

	&movdqa		($t0,@X[7]);
	&psrlq		($t2,19);
	&psllq		($t0,64-61);
	&pxor		($t1,$t2);
	&psrlq		($t2,61-19);
	&pxor		($t1,$t0);
	&psllq		($t0,61-19);
	&pxor		($t1,$t2);
	&movdqa		($t2,&QWP(16*(($j+2)%4),$frame));                   
	&pxor		($t1,$t0);			                 
	&movdqa		($t0,&QWP(16*($j%8),$K512));
	 eval(shift(@insns));
	&paddq		(@X[0],$t1);			                              
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&paddq		($t0,@X[0]);
	 foreach(@insns) { eval; }
	&movdqa		(&QWP(16*($j%8)-128,$frame),$t0);                

	push(@X,shift(@X));				            
    }
	&lea		($K512,&DWP(16*8,$K512));
	&dec		("ecx");
	&jnz		(&label("00_47_ssse3"));

	&movdqa		(@X[1],&QWP(0,$K512));		                
	&lea		($K512,&DWP(-80*8,$K512));	        
	&movdqu		(@X[0],&QWP(0,"ebx"));
	&pshufb		(@X[0],@X[1]);

    for ($j=0;$j<8;$j++) {	                         
	my @insns = (&BODY_00_15_ssse3(),&BODY_00_15_ssse3());

	&movdqa		(&QWP(16*(($j-1)%4),$frame),@X[3])	if ($j>4);           
	&movdqa		(@X[3],&QWP(16*($j%8),$K512));
	&movdqa		(@X[2],@X[1])				if ($j<7);                            
	&movdqu		(@X[1],&QWP(16*($j+1),"ebx"))		if ($j<7);             
	&movdqa		(@X[1],&QWP(16*(($j+1)%4),$frame))	if ($j==7);               
	&paddq		(@X[3],@X[0]);
	&pshufb		(@X[1],@X[2])				if ($j<7);
	 foreach(@insns) { eval; }
	&movdqa		(&QWP(16*($j%8)-128,$frame),@X[3]);                

	push(@X,shift(@X));				            
    }

	                                
	&movq	("mm1",$Bsse2);
	&paddq	($A,"mm3");			                 
	                     
	&movq	("mm3",$Dsse2);
	                   
	                      
	                      
	&movq	("mm7",$Hsse2);

	&pxor	($BxC,"mm1");			          
	&paddq	($A,&QWP(0,"esi"));
	&paddq	("mm1",&QWP(8,"esi"));
	&paddq	($BxC,&QWP(16,"esi"));
	&paddq	("mm3",&QWP(24,"esi"));
	&paddq	($E,&QWP(32,"esi"));
	&paddq	("mm5",&QWP(40,"esi"));
	&paddq	("mm6",&QWP(48,"esi"));
	&paddq	("mm7",&QWP(56,"esi"));

	&movq	(&QWP(0,"esi"),$A);
	&movq	(&QWP(8,"esi"),"mm1");
	&movq	(&QWP(16,"esi"),$BxC);
	&movq	(&QWP(24,"esi"),"mm3");
	&movq	(&QWP(32,"esi"),$E);
	&movq	(&QWP(40,"esi"),"mm5");
	&movq	(&QWP(48,"esi"),"mm6");
	&movq	(&QWP(56,"esi"),"mm7");

    	&cmp	("edi","eax")			                  
	&jb	(&label("loop_ssse3"));

	&mov	("esp",&DWP(64+12,$frame));	            
	&emms	();
}
&function_end("sha512_block_data_order_ssse3");
}

&set_label("K512",64);	                                     
	&data_word(0xd728ae22,0x428a2f98);	     
	&data_word(0x23ef65cd,0x71374491);	     
	&data_word(0xec4d3b2f,0xb5c0fbcf);	     
	&data_word(0x8189dbbc,0xe9b5dba5);	     
	&data_word(0xf348b538,0x3956c25b);	     
	&data_word(0xb605d019,0x59f111f1);	     
	&data_word(0xaf194f9b,0x923f82a4);	     
	&data_word(0xda6d8118,0xab1c5ed5);	     
	&data_word(0xa3030242,0xd807aa98);	     
	&data_word(0x45706fbe,0x12835b01);	     
	&data_word(0x4ee4b28c,0x243185be);	     
	&data_word(0xd5ffb4e2,0x550c7dc3);	     
	&data_word(0xf27b896f,0x72be5d74);	     
	&data_word(0x3b1696b1,0x80deb1fe);	     
	&data_word(0x25c71235,0x9bdc06a7);	     
	&data_word(0xcf692694,0xc19bf174);	     
	&data_word(0x9ef14ad2,0xe49b69c1);	     
	&data_word(0x384f25e3,0xefbe4786);	     
	&data_word(0x8b8cd5b5,0x0fc19dc6);	     
	&data_word(0x77ac9c65,0x240ca1cc);	     
	&data_word(0x592b0275,0x2de92c6f);	     
	&data_word(0x6ea6e483,0x4a7484aa);	     
	&data_word(0xbd41fbd4,0x5cb0a9dc);	     
	&data_word(0x831153b5,0x76f988da);	     
	&data_word(0xee66dfab,0x983e5152);	     
	&data_word(0x2db43210,0xa831c66d);	     
	&data_word(0x98fb213f,0xb00327c8);	     
	&data_word(0xbeef0ee4,0xbf597fc7);	     
	&data_word(0x3da88fc2,0xc6e00bf3);	     
	&data_word(0x930aa725,0xd5a79147);	     
	&data_word(0xe003826f,0x06ca6351);	     
	&data_word(0x0a0e6e70,0x14292967);	     
	&data_word(0x46d22ffc,0x27b70a85);	     
	&data_word(0x5c26c926,0x2e1b2138);	     
	&data_word(0x5ac42aed,0x4d2c6dfc);	     
	&data_word(0x9d95b3df,0x53380d13);	     
	&data_word(0x8baf63de,0x650a7354);	     
	&data_word(0x3c77b2a8,0x766a0abb);	     
	&data_word(0x47edaee6,0x81c2c92e);	     
	&data_word(0x1482353b,0x92722c85);	     
	&data_word(0x4cf10364,0xa2bfe8a1);	     
	&data_word(0xbc423001,0xa81a664b);	     
	&data_word(0xd0f89791,0xc24b8b70);	     
	&data_word(0x0654be30,0xc76c51a3);	     
	&data_word(0xd6ef5218,0xd192e819);	     
	&data_word(0x5565a910,0xd6990624);	     
	&data_word(0x5771202a,0xf40e3585);	     
	&data_word(0x32bbd1b8,0x106aa070);	     
	&data_word(0xb8d2d0c8,0x19a4c116);	     
	&data_word(0x5141ab53,0x1e376c08);	     
	&data_word(0xdf8eeb99,0x2748774c);	     
	&data_word(0xe19b48a8,0x34b0bcb5);	     
	&data_word(0xc5c95a63,0x391c0cb3);	     
	&data_word(0xe3418acb,0x4ed8aa4a);	     
	&data_word(0x7763e373,0x5b9cca4f);	     
	&data_word(0xd6b2b8a3,0x682e6ff3);	     
	&data_word(0x5defb2fc,0x748f82ee);	     
	&data_word(0x43172f60,0x78a5636f);	     
	&data_word(0xa1f0ab72,0x84c87814);	     
	&data_word(0x1a6439ec,0x8cc70208);	     
	&data_word(0x23631e28,0x90befffa);	     
	&data_word(0xde82bde9,0xa4506ceb);	     
	&data_word(0xb2c67915,0xbef9a3f7);	     
	&data_word(0xe372532b,0xc67178f2);	     
	&data_word(0xea26619c,0xca273ece);	     
	&data_word(0x21c0c207,0xd186b8c7);	     
	&data_word(0xcde0eb1e,0xeada7dd6);	     
	&data_word(0xee6ed178,0xf57d4f7f);	     
	&data_word(0x72176fba,0x06f067aa);	     
	&data_word(0xa2c898a6,0x0a637dc5);	     
	&data_word(0xbef90dae,0x113f9804);	     
	&data_word(0x131c471b,0x1b710b35);	     
	&data_word(0x23047d84,0x28db77f5);	     
	&data_word(0x40c72493,0x32caab7b);	     
	&data_word(0x15c9bebc,0x3c9ebe0a);	     
	&data_word(0x9c100d4c,0x431d67c4);	     
	&data_word(0xcb3e42b6,0x4cc5d4be);	     
	&data_word(0xfc657e2a,0x597f299c);	     
	&data_word(0x3ad6faec,0x5fcb6fab);	     
	&data_word(0x4a475817,0x6c44198c);	     

	&data_word(0x04050607,0x00010203);	           
	&data_word(0x0c0d0e0f,0x08090a0b);	      
&asciz("SHA512 block transform for x86, CRYPTOGAMS by <appro\@openssl.org>");

&asm_finish();

close STDOUT or die "error closing STDOUT: $!";
