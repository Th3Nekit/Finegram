#! /usr/bin/env perl
# Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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


$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
push(@INC,"${dir}","${dir}../../../perlasm");
require "x86asm.pl";

$output = pop;
open STDOUT,">$output";

&asm_init($ARGV[0]);

$sse2=1;

&bn_mul_add_words("bn_mul_add_words");
&bn_mul_words("bn_mul_words");
&bn_sqr_words("bn_sqr_words");
&bn_add_words("bn_add_words");
&bn_sub_words("bn_sub_words");

&asm_finish();

close STDOUT or die "error closing STDOUT: $!";

sub bn_mul_add_words
	{
	local($name)=@_;

	&function_begin_B($name);

	$r="eax";
	$a="edx";
	$c="ecx";

	if ($sse2) {
		&mov($r,&wparam(0));
		&mov($a,&wparam(1));
		&mov($c,&wparam(2));
		&movd("mm0",&wparam(3));	         
		&pxor("mm1","mm1");		                
		&jmp(&label("maw_sse2_entry"));

	&set_label("maw_sse2_unrolled",16);
		&movd("mm3",&DWP(0,$r,"",0));	            
		&paddq("mm1","mm3");		                       
		&movd("mm2",&DWP(0,$a,"",0));	            
		&pmuludq("mm2","mm0");		              
		&movd("mm4",&DWP(4,$a,"",0));	            
		&pmuludq("mm4","mm0");		              
		&movd("mm6",&DWP(8,$a,"",0));	            
		&pmuludq("mm6","mm0");		              
		&movd("mm7",&DWP(12,$a,"",0));	            
		&pmuludq("mm7","mm0");		              
		&paddq("mm1","mm2");		                                
		&movd("mm3",&DWP(4,$r,"",0));	            
		&paddq("mm3","mm4");		                     
		&movd("mm5",&DWP(8,$r,"",0));	            
		&paddq("mm5","mm6");		                     
		&movd("mm4",&DWP(12,$r,"",0));	            
		&paddq("mm7","mm4");		                     
		&movd(&DWP(0,$r,"",0),"mm1");
		&movd("mm2",&DWP(16,$a,"",0));	            
		&pmuludq("mm2","mm0");		              
		&psrlq("mm1",32);		              
		&movd("mm4",&DWP(20,$a,"",0));	            
		&pmuludq("mm4","mm0");		              
		&paddq("mm1","mm3");		                              
		&movd("mm6",&DWP(24,$a,"",0));	            
		&pmuludq("mm6","mm0");		              
		&movd(&DWP(4,$r,"",0),"mm1");
		&psrlq("mm1",32);		              
		&movd("mm3",&DWP(28,$a,"",0));	            
		&add($a,32);
		&pmuludq("mm3","mm0");		              
		&paddq("mm1","mm5");		                              
		&movd("mm5",&DWP(16,$r,"",0));	            
		&paddq("mm2","mm5");		                     
		&movd(&DWP(8,$r,"",0),"mm1");
		&psrlq("mm1",32);		              
		&paddq("mm1","mm7");		                              
		&movd("mm5",&DWP(20,$r,"",0));	            
		&paddq("mm4","mm5");		                     
		&movd(&DWP(12,$r,"",0),"mm1");
		&psrlq("mm1",32);		              
		&paddq("mm1","mm2");		                              
		&movd("mm5",&DWP(24,$r,"",0));	            
		&paddq("mm6","mm5");		                     
		&movd(&DWP(16,$r,"",0),"mm1");
		&psrlq("mm1",32);		              
		&paddq("mm1","mm4");		                              
		&movd("mm5",&DWP(28,$r,"",0));	            
		&paddq("mm3","mm5");		                     
		&movd(&DWP(20,$r,"",0),"mm1");
		&psrlq("mm1",32);		              
		&paddq("mm1","mm6");		                              
		&movd(&DWP(24,$r,"",0),"mm1");
		&psrlq("mm1",32);		              
		&paddq("mm1","mm3");		                              
		&movd(&DWP(28,$r,"",0),"mm1");
		&lea($r,&DWP(32,$r));
		&psrlq("mm1",32);		                 

		&sub($c,8);
		&jz(&label("maw_sse2_exit"));
	&set_label("maw_sse2_entry");
		&test($c,0xfffffff8);
		&jnz(&label("maw_sse2_unrolled"));

	&set_label("maw_sse2_loop",4);
		&movd("mm2",&DWP(0,$a));	            
		&movd("mm3",&DWP(0,$r));	            
		&pmuludq("mm2","mm0");		           
		&lea($a,&DWP(4,$a));
		&paddq("mm1","mm3");		               
		&paddq("mm1","mm2");		                 
		&movd(&DWP(0,$r),"mm1");	                  
		&sub($c,1);
		&psrlq("mm1",32);		                    
		&lea($r,&DWP(4,$r));
		&jnz(&label("maw_sse2_loop"));
	&set_label("maw_sse2_exit");
		&movd("eax","mm1");		               
		&emms();
		&ret();
	}
	&function_end($name);
	}

sub bn_mul_words
	{
	local($name)=@_;

	&function_begin_B($name);

	$r="eax";
	$a="edx";
	$c="ecx";

	if ($sse2) {
		&mov($r,&wparam(0));
		&mov($a,&wparam(1));
		&mov($c,&wparam(2));
		&movd("mm0",&wparam(3));	         
		&pxor("mm1","mm1");		                 

	&set_label("mw_sse2_loop",16);
		&movd("mm2",&DWP(0,$a));	            
		&pmuludq("mm2","mm0");		           
		&lea($a,&DWP(4,$a));
		&paddq("mm1","mm2");		                 
		&movd(&DWP(0,$r),"mm1");	                  
		&sub($c,1);
		&psrlq("mm1",32);		                    
		&lea($r,&DWP(4,$r));
		&jnz(&label("mw_sse2_loop"));

		&movd("eax","mm1");		              
		&emms();
		&ret();
	}
	&function_end($name);
	}

sub bn_sqr_words
	{
	local($name)=@_;

	&function_begin_B($name);

	$r="eax";
	$a="edx";
	$c="ecx";

	if ($sse2) {
		&mov($r,&wparam(0));
		&mov($a,&wparam(1));
		&mov($c,&wparam(2));

	&set_label("sqr_sse2_loop",16);
		&movd("mm0",&DWP(0,$a));	            
		&pmuludq("mm0","mm0");		              
		&lea($a,&DWP(4,$a));		     
		&movq(&QWP(0,$r),"mm0");	                  
		&sub($c,1);
		&lea($r,&DWP(8,$r));		        
		&jnz(&label("sqr_sse2_loop"));

		&emms();
		&ret();
	}
	&function_end($name);
	}

sub bn_add_words
	{
	local($name)=@_;

	&function_begin($name,"");

	&comment("");
	$a="esi";
	$b="edi";
	$c="eax";
	$r="ebx";
	$tmp1="ecx";
	$tmp2="edx";
	$num="ebp";

	&mov($r,&wparam(0));	       
	 &mov($a,&wparam(1));	       
	&mov($b,&wparam(2));	       
	 &mov($num,&wparam(3));	         
	&xor($c,$c);		             
	 &and($num,0xfffffff8);	         

	&jz(&label("aw_finish"));

	&set_label("aw_loop",0);
	for ($i=0; $i<8; $i++)
		{
		&comment("Round $i");

		&mov($tmp1,&DWP($i*4,$a,"",0)); 	    
		 &mov($tmp2,&DWP($i*4,$b,"",0)); 	    
		&add($tmp1,$c);
		 &mov($c,0);
		&adc($c,$c);
		 &add($tmp1,$tmp2);
		&adc($c,0);
		 &mov(&DWP($i*4,$r,"",0),$tmp1); 	    
		}

	&comment("");
	&add($a,32);
	 &add($b,32);
	&add($r,32);
	 &sub($num,8);
	&jnz(&label("aw_loop"));

	&set_label("aw_finish",0);
	&mov($num,&wparam(3));	         
	&and($num,7);
	 &jz(&label("aw_end"));

	for ($i=0; $i<7; $i++)
		{
		&comment("Tail Round $i");
		&mov($tmp1,&DWP($i*4,$a,"",0));	    
		 &mov($tmp2,&DWP($i*4,$b,"",0));    
		&add($tmp1,$c);
		 &mov($c,0);
		&adc($c,$c);
		 &add($tmp1,$tmp2);
		&adc($c,0);
		 &dec($num) if ($i != 6);
		&mov(&DWP($i*4,$r,"",0),$tmp1);	    
		 &jz(&label("aw_end")) if ($i != 6);
		}
	&set_label("aw_end",0);

                                

	&function_end($name);
	}

sub bn_sub_words
	{
	local($name)=@_;

	&function_begin($name,"");

	&comment("");
	$a="esi";
	$b="edi";
	$c="eax";
	$r="ebx";
	$tmp1="ecx";
	$tmp2="edx";
	$num="ebp";

	&mov($r,&wparam(0));	       
	 &mov($a,&wparam(1));	       
	&mov($b,&wparam(2));	       
	 &mov($num,&wparam(3));	         
	&xor($c,$c);		             
	 &and($num,0xfffffff8);	         

	&jz(&label("aw_finish"));

	&set_label("aw_loop",0);
	for ($i=0; $i<8; $i++)
		{
		&comment("Round $i");

		&mov($tmp1,&DWP($i*4,$a,"",0)); 	    
		 &mov($tmp2,&DWP($i*4,$b,"",0)); 	    
		&sub($tmp1,$c);
		 &mov($c,0);
		&adc($c,$c);
		 &sub($tmp1,$tmp2);
		&adc($c,0);
		 &mov(&DWP($i*4,$r,"",0),$tmp1); 	    
		}

	&comment("");
	&add($a,32);
	 &add($b,32);
	&add($r,32);
	 &sub($num,8);
	&jnz(&label("aw_loop"));

	&set_label("aw_finish",0);
	&mov($num,&wparam(3));	         
	&and($num,7);
	 &jz(&label("aw_end"));

	for ($i=0; $i<7; $i++)
		{
		&comment("Tail Round $i");
		&mov($tmp1,&DWP($i*4,$a,"",0));	    
		 &mov($tmp2,&DWP($i*4,$b,"",0));    
		&sub($tmp1,$c);
		 &mov($c,0);
		&adc($c,$c);
		 &sub($tmp1,$tmp2);
		&adc($c,0);
		 &dec($num) if ($i != 6);
		&mov(&DWP($i*4,$r,"",0),$tmp1);	    
		 &jz(&label("aw_end")) if ($i != 6);
		}
	&set_label("aw_end",0);

                                

	&function_end($name);
	}
