// Copyright 2022 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import std;

import xls.examples.ram;

// TTU registers.
const T0 = u2:0;
const T1 = u2:1;
const T2 = u2:2;
const T3 = u2:3;

// Instructions.
const LOAD = u8:0;
const STORE = u8:1;
const MUL = u8:2;
const ADD = u8:3;
const MOVHI = u8:4;
const MOVLO = u8:5;
const ZERO_INIT = u8:6;
const END = u8:255;

// Regular ALU registers.
const R0 = u2:0;
const R1 = u2:1;
const R2 = u2:2;
const R3 = u2:3;

pub struct LoopBundle {
    start: u32,
    end: u32,
    stride: u16,
}

pub struct TtuBundle<NumLoops: u32> {
    loops: LoopBundle[NumLoops],
    base: u32,
}

pub struct TtuInstruction<NumTtus: u32> {
    ttu: TtuBundle<2>[NumTtus],
}

pub struct TtuState<NumTtus: u32, NumLoops: u32> {
    loopnest_state: u32[NumLoops][NumTtus],
}

pub struct Register<Bits: u32> {
    is_ttu_reg: u1,
    reg: bits[Bits],
}

pub struct Instruction {
    opcode: u8,
    src_reg0: Register<2>,
    src_reg1: Register<2>,
    dst_reg0: Register<2>,
    dst_reg1: Register<2>,
}

pub struct Bundle {
    load_store_slot: Instruction,
    alu_slot: Instruction,
    imm_slot: Instruction,
    ttu_slot0: Instruction,
    ttu_slot1: Instruction,
    ttu_tick: u1,
}

// Creating a proc that consumes a TTU bundle.
proc ttu {
   instruction : chan<TtuInstruction<3>> in;
   tick: chan<bool> in;
   result: chan<u32[3]> out;

   init { TtuState<3, 2>{loopnest_state: u32[2][3]: [[u32:0, u32:0], ...]} }

   config(instruction: chan<TtuInstruction<3>> in, tick: chan<bool> in, result: chan<u32[3]> out) {
    (instruction, tick, result)
   }

    next(tok: token, state: TtuState<3, 2>) {
        // Just tying things up to compile.
        let (tok, ins) = recv(tok, instruction);
        let (tok, one_tick) = recv(tok, tick);
        let outer_ticks = ins.ttu[0].loops[0].end - ins.ttu[0].loops[0].start;
        let inner_ticks = ins.ttu[0].loops[1].end - ins.ttu[0].loops[1].start;
        let outer_stride = ins.ttu[0].loops[1].stride;
        let inner_stride = ins.ttu[0].loops[0].stride;

        for (i, os): (u32, TtuState<3, 2>) in range(u32:0, outer_ticks) {
            for (j, is): (u32, TtuState<3, 2>) in range(u32:0, inner_ticks) {
                let a = for (k, addresses): (u32, u32[3]) in range(u32:0, 3) {
                    let base = ins.ttu[k].base;
                    let outer_stride = ins.ttu[k].loops[1].stride;
                    let inner_stride = ins.ttu[k].loops[0].stride;
                    let address = i * outer_stride + j * inner_stride + base;
                }([0,...]);
                let tok = send(tok, result, a);
            }(os)
        }(state)
    }

}


#[test_proc]
proc main {
    terminator: chan<bool> out;

    init {()}

    config(terminator: chan<bool> out) {
        let loop = LoopBundle {start:u32: 0, end: u32: 10, stride: u16:1};
        assert_eq(loop.start, u32:0);
        (terminator,)
    }
    next(tok: token, state: ()) {
        let tok = send(tok, terminator, true);
    }
}
