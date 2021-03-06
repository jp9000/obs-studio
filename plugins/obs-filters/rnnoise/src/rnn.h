/* Copyright (c) 2017 Jean-Marc Valin */
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

#ifndef RNN_H_
#define RNN_H_

#include "rnnoise.h"

#include "opus_types.h"

#define WEIGHTS_SCALE (1.f/256)

#define MAX_NEURONS 128

#define ACTIVATION_TANH    0
#define ACTIVATION_SIGMOID 1
#define ACTIVATION_RELU    2

typedef signed char rnn_weight;

typedef struct {
  const rnn_weight *bias;
  const rnn_weight *input_weights;
  int nb_inputs;
  int nb_neurons;
  int activation;
} DenseLayer;

typedef struct {
  const rnn_weight *bias;
  const rnn_weight *input_weights;
  const rnn_weight *recurrent_weights;
  int nb_inputs;
  int nb_neurons;
  int activation;
} GRULayer;

typedef struct RNNState RNNState;

void compute_rnn(RNNState *rnn, float *gains, float *vad, const float *input);

#endif /* _MLP_H_ */
