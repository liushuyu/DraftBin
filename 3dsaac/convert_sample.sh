#!/bin/bash

cd -- "$(dirname $0)"
xxd -i assets/Fastigium\ -\ Come\ In.aac > source/audio_samples.h
