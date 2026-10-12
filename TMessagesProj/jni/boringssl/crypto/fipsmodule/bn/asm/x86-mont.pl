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


# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project.
# ====================================================================

# October 2005
#
# This is a "teaser" code, as it can be improved in several ways...
# First of all non-SSE2 path should be implemented (yes, for now it
# performs Montgomery multiplication/convolution only on SSE2-capable
# CPUs such as P4, others fall down to original code). Then inner loop
# can be unrolled and modulo-scheduled to improve ILP and possibly
# moved to 128-bit XMM register bank (though it would require input
# rearrangement and/or increase bus bandwidth utilization). Dedicated
# squaring procedure should give further performance improvement...
# Yet, for being draft, the code improves rsa512 *sign* benchmark by
# 110%(!), rsa1024 one - by 70% and rsa4096 - by 20%:-)

# December 2006
#
# Modulo-scheduling SSE2 loops results in further 15-20% improvement.
# Integer-only code [being equipped with dedicated squaring procedure]
# gives ~40% on rsa512 sign benchmark...

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC,"${dir}","${dir}../../../perlasm");
require "x86asm.pl";

$output = pop;
open STDOUT,">$output";

&asm_init($ARGV[0]);

$sse2=1;

&function_begin("bn_mul_mont");

$i="edx";
$j="ecx";
$ap="esi";	$tp="esi";		                          
$rp="edi";	$bp="edi";		                          
$np="ebp";
$num="ebx";

$_num=&DWP(4*0,"esp");			                  
$_rp=&DWP(4*1,"esp");
$_ap=&DWP(4*2,"esp");
$_bp=&DWP(4*3,"esp");
$_np=&DWP(4*4,"esp");
$_n0=&DWP(4*5,"esp");	$_n0q=&QWP(4*5,"esp");
$_sp=&DWP(4*6,"esp");
$_bpend=&DWP(4*7,"esp");
$frame=32;				                                       

	                                                           
	&mov	("edi",&wparam(5));	         
	                  
	&lea	("esi",&wparam(0));	                                     
	&lea	("edx",&wparam(1));	         
	&add	("edi",2);		                              
	&neg	("edi");
	&lea	("ebp",&DWP(-$frame,"esp","edi",4));	                                 
	&neg	("edi");

	                                                                
	                                                                
	                                                            
	        
	&mov	("eax","ebp");
	&sub	("eax","edx");
	&and	("eax",2047);
	&sub	("ebp","eax");		                                   

	&xor	("edx","ebp");
	&and	("edx",2048);
	&xor	("edx",2048);
	&sub	("ebp","edx");		                                    

	&and	("ebp",-64);		                     

	                                     
	 
	                                                      
	                                                              
	                                                               
	                                                             
	                                                            
	                                                             
	&mov	("eax","esp");
	&sub	("eax","ebp");
	&and	("eax",-4096);
	&mov	("edx","esp");		                      
	&lea	("esp",&DWP(0,"ebp","eax"));
	&mov	("eax",&DWP(0,"esp"));
	&cmp	("esp","ebp");
	&ja	(&label("page_walk"));
	&jmp	(&label("page_walk_done"));

&set_label("page_walk",16);
	&lea	("esp",&DWP(-4096,"esp"));
	&mov	("eax",&DWP(0,"esp"));
	&cmp	("esp","ebp");
	&ja	(&label("page_walk"));
&set_label("page_walk_done");

	                                                        
	&mov	("eax",&DWP(0*4,"esi"));              
	&mov	("ebx",&DWP(1*4,"esi"));                    
	&mov	("ecx",&DWP(2*4,"esi"));                    
	&mov	("ebp",&DWP(3*4,"esi"));                    
	&mov	("esi",&DWP(4*4,"esi"));                    
	                                       

	&mov	("esi",&DWP(0,"esi"));	            
	&mov	($_rp,"eax");		                                   
	&mov	($_ap,"ebx");
	&mov	($_bp,"ecx");
	&mov	($_np,"ebp");
	&mov	($_n0,"esi");
	&lea	($num,&DWP(-3,"edi"));	                                       
	                                                      
	&mov	($_sp,"edx");		                      

if($sse2) {
$acc0="mm0";	                          
$acc1="mm1";
$car0="mm2";
$car1="mm3";
$mul0="mm4";
$mul1="mm5";
$temp="mm6";
$mask="mm7";

	&mov	("eax",-1);
	&movd	($mask,"eax");		                    

	&mov	($ap,$_ap);		                     
	&mov	($bp,$_bp);
	&mov	($np,$_np);

	&xor	($i,$i);		     
	&xor	($j,$j);		     

	&movd	($mul0,&DWP(0,$bp));		       
	&movd	($mul1,&DWP(0,$ap));		       
	&movd	($car1,&DWP(0,$np));		       

	&pmuludq($mul1,$mul0);			             
	&movq	($car0,$mul1);
	&movq	($acc0,$mul1);			                        
	&pand	($acc0,$mask);			                          

	&pmuludq($mul1,$_n0q);			      

	&pmuludq($car1,$mul1);			                 
	&paddq	($car1,$acc0);

	&movd	($acc1,&DWP(4,$np));		       
	&movd	($acc0,&DWP(4,$ap));		       

	&psrlq	($car0,32);
	&psrlq	($car1,32);

	&inc	($j);				     
&set_label("1st",16);
	&pmuludq($acc0,$mul0);			             
	&pmuludq($acc1,$mul1);			          
	&paddq	($car0,$acc0);			      
	&paddq	($car1,$acc1);			      

	&movq	($acc0,$car0);
	&pand	($acc0,$mask);
	&movd	($acc1,&DWP(4,$np,$j,4));	         
	&paddq	($car1,$acc0);			                
	&movd	($acc0,&DWP(4,$ap,$j,4));	         
	&psrlq	($car0,32);
	&movd	(&DWP($frame-4,"esp",$j,4),$car1);	          
	&psrlq	($car1,32);

	&lea	($j,&DWP(1,$j));
	&cmp	($j,$num);
	&jl	(&label("1st"));

	&pmuludq($acc0,$mul0);			                 
	&pmuludq($acc1,$mul1);			              
	&paddq	($car0,$acc0);			      
	&paddq	($car1,$acc1);			      

	&movq	($acc0,$car0);
	&pand	($acc0,$mask);
	&paddq	($car1,$acc0);			                    
	&movd	(&DWP($frame-4,"esp",$j,4),$car1);	            

	&psrlq	($car0,32);
	&psrlq	($car1,32);

	&paddq	($car1,$car0);
	&movq	(&QWP($frame,"esp",$num,4),$car1);	                   

	&inc	($i);				     
&set_label("outer");
	&xor	($j,$j);			     

	&movd	($mul0,&DWP(0,$bp,$i,4));	       
	&movd	($mul1,&DWP(0,$ap));		       
	&movd	($temp,&DWP($frame,"esp"));	       
	&movd	($car1,&DWP(0,$np));		       
	&pmuludq($mul1,$mul0);			             

	&paddq	($mul1,$temp);			         
	&movq	($acc0,$mul1);
	&movq	($car0,$mul1);
	&pand	($acc0,$mask);

	&pmuludq($mul1,$_n0q);			      

	&pmuludq($car1,$mul1);
	&paddq	($car1,$acc0);

	&movd	($temp,&DWP($frame+4,"esp"));	       
	&movd	($acc1,&DWP(4,$np));		       
	&movd	($acc0,&DWP(4,$ap));		       

	&psrlq	($car0,32);
	&psrlq	($car1,32);
	&paddq	($car0,$temp);			         

	&inc	($j);				     
	&dec	($num);
&set_label("inner");
	&pmuludq($acc0,$mul0);			             
	&pmuludq($acc1,$mul1);			          
	&paddq	($car0,$acc0);			      
	&paddq	($car1,$acc1);			      

	&movq	($acc0,$car0);
	&movd	($temp,&DWP($frame+4,"esp",$j,4));         
	&pand	($acc0,$mask);
	&movd	($acc1,&DWP(4,$np,$j,4));	         
	&paddq	($car1,$acc0);			                     
	&movd	($acc0,&DWP(4,$ap,$j,4));	         
	&psrlq	($car0,32);
	&movd	(&DWP($frame-4,"esp",$j,4),$car1);          
	&psrlq	($car1,32);
	&paddq	($car0,$temp);			           

	&dec	($num);
	&lea	($j,&DWP(1,$j));		     
	&jnz	(&label("inner"));

	&mov	($num,$j);
	&pmuludq($acc0,$mul0);			                 
	&pmuludq($acc1,$mul1);			              
	&paddq	($car0,$acc0);			      
	&paddq	($car1,$acc1);			      

	&movq	($acc0,$car0);
	&pand	($acc0,$mask);
	&paddq	($car1,$acc0);			                             
	&movd	(&DWP($frame-4,"esp",$j,4),$car1);	            
	&psrlq	($car0,32);
	&psrlq	($car1,32);

	&movd	($temp,&DWP($frame+4,"esp",$num,4));	            
	&paddq	($car1,$car0);
	&paddq	($car1,$temp);
	&movq	(&QWP($frame,"esp",$num,4),$car1);	                   

	&lea	($i,&DWP(1,$i));		     
	&cmp	($i,$num);
	&jle	(&label("outer"));

	&emms	();				                    
	&jmp	(&label("common_tail"));
}

&set_label("common_tail",16);
	&mov	($np,$_np);			                      
	&mov	($rp,$_rp);			                     
	&lea	($tp,&DWP($frame,"esp"));	                          

	&mov	("eax",&DWP(0,$tp));		       
	&mov	($j,$num);			         
	&xor	($i,$i);			                   

&set_label("sub",16);
	&sbb	("eax",&DWP(0,$np,$i,4));
	&mov	(&DWP(0,$rp,$i,4),"eax");	                   
	&dec	($j);				                    
	&mov	("eax",&DWP(4,$tp,$i,4));	         
	&lea	($i,&DWP(1,$i));		     
	&jge	(&label("sub"));

	&sbb	("eax",0);			                            
	&mov	("edx",-1);
	&xor	("edx","eax");
	&jmp	(&label("copy"));

&set_label("copy",16);				                  
	&mov	($tp,&DWP($frame,"esp",$num,4));
	&mov	($np,&DWP(0,$rp,$num,4));
	&mov	(&DWP($frame,"esp",$num,4),$j);	                      
	&and	($tp,"eax");
	&and	($np,"edx");
	&or	($np,$tp);
	&mov	(&DWP(0,$rp,$num,4),$np);
	&dec	($num);
	&jge	(&label("copy"));

	&mov	("esp",$_sp);		                          
	                 
&function_end("bn_mul_mont");

&asciz("Montgomery Multiplication for x86, CRYPTOGAMS by <appro\@openssl.org>");

&asm_finish();

close STDOUT or die "error closing STDOUT: $!";
