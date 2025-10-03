# Copyright (c) 2015-2019, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# libnvdla_compiler
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE_CC := gcc
MODULE_CPP := g++
MODULE_LD := ld

NVDLA_SRC_FILES := \
    caffe/CaffeParser.cc \
    caffe/ditcaffe/protobuf-2.6.1/ditcaffe.pb.cc \
    engine-ast/ActivationOp.cc \
    engine-ast/BatchNormOp.cc \
    engine-ast/BDMANode.cc \
    engine-ast/BDMASingleOp.cc \
    engine-ast/BDMAGroupOp.cc \
    engine-ast/BiasOp.cc \
    engine-ast/CDPLRNOp.cc \
    engine-ast/CDPNode.cc \
    engine-ast/ConcatOp.cc \
    engine-ast/ConvCoreNode.cc \
    engine-ast/ConvolutionOp.cc \
    engine-ast/CPUNode.cc \
    engine-ast/DeconvolutionOp.cc \
    engine-ast/EngineAST.cc \
    engine-ast/EngineEdge.cc \
    engine-ast/EngineGraph.cc \
    engine-ast/EngineNode.cc \
    engine-ast/EngineNodeFactory.cc \
    engine-ast/FullyConnectedOp.cc \
    engine-ast/MultiOpsNode.cc \
    engine-ast/NestedGraph.cc \
    engine-ast/PDPNode.cc \
    engine-ast/RubikNode.cc \
    engine-ast/ScaleOp.cc \
    engine-ast/SDPEltWiseOp.cc \
    engine-ast/SDPNode.cc \
    engine-ast/SDPNOP.cc \
    engine-ast/SDPSuperOp.cc \
    engine-ast/SoftMaxOp.cc \
    engine-ast/SplitOp.cc \
    AST.cc \
    CanonicalAST.cc \
    Check.cc \
    Compiler.cc \
    DlaPrototestInterface.pb.cc \
    DLAResourceManager.cc \
    DLAInterface.cc \
    DLAInterfaceA.cc \
    $(ROOT)/core/src/common/EMUInterface.cc \
    $(ROOT)/core/src/common/EMUInterfaceA.cc \
    Layer.cc \
    $(ROOT)/core/src/common/Loadable.cc \
    LutManager.cc \
    Memory.cc \
    Network.cc \
    Profile.cc \
    Profiler.cc \
    Setup.cc \
    Surface.cc \
    TargetConfig.cc \
    Tensor.cc \
    TestPointParameter.cc \
    Wisdom.cc \
    WisdomContainer.cc \
    $(ROOT)/utils/BitBinaryTree.c \
    $(ROOT)/utils/BuddyAlloc.c \
    $(ROOT)/utils/ErrorLogging.c \
    $(ROOT)/port/linux/nvdla_os.c

INCLUDES += \
    -I$(LOCAL_DIR)/include \
    -I$(ROOT)/include \
    -I$(ROOT)/core/include \
    -I$(ROOT)/external/include \
    -I$(ROOT)/port/linux/include \
    -I$(ROOT)/core/src/common/include \
    -I${PROTOBUF_INSTALL_DIR}/include \

MODULE_CPPFLAGS += \
    -DNVDLA_UTILS_ERROR_TAG="\"DLA\"" \
    -DGOOGLE_PROTOBUF_NO_RTTI \
    -DNVDLA_COMPILER_OUTPUT_FOR_PROTOTEST \

MODULE_CFLAGS += \
    -DNVDLA_UTILS_ERROR_TAG="\"DLA\"" \

MODULE_SRCS := $(NVDLA_SRC_FILES)

include $(ROOT)/make/module.mk
