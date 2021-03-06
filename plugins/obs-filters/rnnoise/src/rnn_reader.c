/* Copyright (c) 2018 Gregor Richards */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rnn.h"
#include "rnn_data.h"
#include "rnnoise.h"

/* Although these values are the same as in rnn.h, we make them separate to
 * avoid accidentally burning internal values into a file format */
#define F_ACTIVATION_TANH       0
#define F_ACTIVATION_SIGMOID    1
#define F_ACTIVATION_RELU       2

RNNModel *rnnoise_model_from_file(FILE *f)
{
    int i, in;

    if (fscanf(f, "rnnoise-nu model file version %d\n", &in) != 1 || in != 1)
        return NULL;

    RNNModel *ret = calloc(1, sizeof(RNNModel));
    if (!ret)
        return NULL;

#define ALLOC_LAYER(type, name) \
    type *name; \
    name = calloc(1, sizeof(type)); \
    if (!name) { \
        rnnoise_model_free(ret); \
        return NULL; \
    } \
    ret->name = name

    ALLOC_LAYER(DenseLayer, input_dense);
    ALLOC_LAYER(GRULayer, vad_gru);
    ALLOC_LAYER(GRULayer, noise_gru);
    ALLOC_LAYER(GRULayer, denoise_gru);
    ALLOC_LAYER(DenseLayer, denoise_output);
    ALLOC_LAYER(DenseLayer, vad_output);

#define INPUT_VAL(name) do { \
    if (fscanf(f, "%d", &in) != 1 || in < 0 || in > 128) { \
        rnnoise_model_free(ret); \
        return NULL; \
    } \
    name = in; \
    } while (0)

#define INPUT_ACTIVATION(name) do { \
    int activation; \
    INPUT_VAL(activation); \
    switch (activation) { \
        case F_ACTIVATION_SIGMOID: \
            name = ACTIVATION_SIGMOID; \
            break; \
        case F_ACTIVATION_RELU: \
            name = ACTIVATION_RELU; \
            break; \
        default: \
            name = ACTIVATION_TANH; \
    } \
    } while (0)

#define INPUT_ARRAY(name, len) do { \
    rnn_weight *values = malloc((len) * sizeof(rnn_weight)); \
    if (!values) { \
        rnnoise_model_free(ret); \
        return NULL; \
    } \
    name = values; \
    for (i = 0; i < (len); i++) { \
        if (fscanf(f, "%d", &in) != 1) { \
            rnnoise_model_free(ret); \
            return NULL; \
        } \
        values[i] = in; \
    } \
    } while (0)

#define INPUT_DENSE(name) do { \
    INPUT_VAL(name->nb_inputs); \
    INPUT_VAL(name->nb_neurons); \
    ret->name ## _size = name->nb_neurons; \
    INPUT_ACTIVATION(name->activation); \
    INPUT_ARRAY(name->input_weights, name->nb_inputs * name->nb_neurons); \
    INPUT_ARRAY(name->bias, name->nb_neurons); \
    } while (0)

#define INPUT_GRU(name) do { \
    INPUT_VAL(name->nb_inputs); \
    INPUT_VAL(name->nb_neurons); \
    ret->name ## _size = name->nb_neurons; \
    INPUT_ACTIVATION(name->activation); \
    INPUT_ARRAY(name->input_weights, name->nb_inputs * name->nb_neurons * 3); \
    INPUT_ARRAY(name->recurrent_weights, name->nb_neurons * name->nb_neurons * 3); \
    INPUT_ARRAY(name->bias, name->nb_neurons * 3); \
    } while (0)

    INPUT_DENSE(input_dense);
    INPUT_GRU(vad_gru);
    INPUT_GRU(noise_gru);
    INPUT_GRU(denoise_gru);
    INPUT_DENSE(denoise_output);
    INPUT_DENSE(vad_output);

    return ret;
}

void rnnoise_model_free(RNNModel *model)
{
#define FREE_MAYBE(ptr) do { if (ptr) free(ptr); } while (0)
#define FREE_DENSE(name) do { \
    if (model->name) { \
        free((void *) model->name->input_weights); \
        free((void *) model->name->bias); \
        free((void *) model->name); \
    } \
    } while (0)
#define FREE_GRU(name) do { \
    if (model->name) { \
        free((void *) model->name->input_weights); \
        free((void *) model->name->recurrent_weights); \
        free((void *) model->name->bias); \
        free((void *) model->name); \
    } \
    } while (0)

    if (!model)
        return;
    FREE_DENSE(input_dense);
    FREE_GRU(vad_gru);
    FREE_GRU(noise_gru);
    FREE_GRU(denoise_gru);
    FREE_DENSE(denoise_output);
    FREE_DENSE(vad_output);
    free(model);
}
