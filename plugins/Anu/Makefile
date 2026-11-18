#!/usr/bin/make -f
# Makefile for the Anu plugin (DPF)

NAME = Anu

FILES_DSP = \
	AnuPlugin.cpp \
	AnuEngine.cpp \
	AnuCore.cpp

FILES_UI = \
	AnuUI.cpp

UI_TYPE = opengl

include ../../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -I../../src -std=gnu++17

TARGETS += vst3
TARGETS += clap
TARGETS += lv2_sep
TARGETS += jack

ifeq ($(MACOS),true)
TARGETS += au
endif

all: $(TARGETS)
