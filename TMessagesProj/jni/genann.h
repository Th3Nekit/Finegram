/*
 * GENANN - Minimal C Artificial Neural Network
 *
 * Copyright (c) 2015, 2016 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#ifndef __GENANN_H__
#define __GENANN_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GENANN_RANDOM

#define GENANN_RANDOM() (((double)rand())/RAND_MAX)
#endif

typedef double (*genann_actfun)(double a);

typedef struct genann {

    int inputs, hidden_layers, hidden, outputs;

    genann_actfun activation_hidden;

    genann_actfun activation_output;

    int total_weights;

    int total_neurons;

    double *weight;

    double *output;

    double *delta;

} genann;

genann *genann_init(int inputs, int hidden_layers, int hidden, int outputs);

genann *genann_read(FILE *in);

void genann_randomize(genann *ann);

genann *genann_copy(genann const *ann);

void genann_free(genann *ann);

double const *genann_run(genann const *ann, double const *inputs);

void genann_train(genann const *ann, double const *inputs, double const *desired_outputs, double learning_rate);

void genann_write(genann const *ann, FILE *out);

double genann_act_sigmoid(double a);
double genann_act_sigmoid_cached(double a);
double genann_act_threshold(double a);
double genann_act_linear(double a);

#ifdef __cplusplus
}
#endif

#endif
