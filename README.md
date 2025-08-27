# cbp_conv

Converter for CBP traces

# File extensions assumptions
```

Inputs
  <none>      assumed to be in CBP binary format
  .cbp        CBP binary format
  .txt        CBP text format
  .jsonl      NDJSON

Outputs
  <none>      CBP binary
  .cbp        CBP binary
  .txt        CBP text
  .jsonl      NDJSON

Output only
  .asm        RISC-V assembly source
  .stf        stf_lib format
  .memh       systemverilog memh format

Compression
  .bz2        bzip2
  .gz         gzip
  .xz         lzma2
```

```
<filename><base input ext><output ext><optional compression ext>
```

Extension case is ignored.

The base input formats are implied CBP (no extension) binary format, 
.cbp explicit CBP binary format, .txt a text form of the CBP format, and
a jsonl form of the CBP format.

If `base input ext` is none of the above, the file is treated as CBP binary
format.

The output formats can be any of the base formats as well as RISC-V
assembly, STF lib output or verilog memh format.

Optionally the inputs and outputs can be compressed. This is expressed by
chaining the input/output extensions with one of the 4 compression forms.

The command line options --in and --out express the conversion input to 
output. 

With exceptions conversion between these formats is supported.  The 
exceptions are asm, stf and memh are output only formats. The output 
only formats can still be compressed or not.

# CBP operation types

CBP is a binary format, the record/op types are listed here, long with
the mnemonic.


```
    InstClass::aluOpClass                    return  "aluOp";
    InstClass::callDirectInstClass:          return  "callDirBrOp";
    InstClass::callIndirectInstClass:        return  "callIndBrOp";
    InstClass::condBranchInstClass:          return  "condBrOp";
    InstClass::fpInstClass:                  return  "fpOp";
    InstClass::loadInstClass:                return  "loadOp";
    InstClass::ReturnInstClass:              return  "retBrOp";
    InstClass::slowAluInstClass:             return  "slowAluOp";
    InstClass::storeInstClass:               return  "stOp";
    InstClass::uncondDirectBranchInstClass:  return  "uncondDirBrOp";
    InstClass::uncondIndirectBranchInstClass:return "uncondIndBrOp";
```

When converting to asm, stf or memh certain RISC-V instruction types are
assumed.

Examples of each operation type in text form and the ASM format shown below.
The ASM form includes an instruction declaration and meta data in comment form.

Note that in no case is a register pre-initialized to the value shown in the 
meta data. This would insert additional instructions into the trace. Instead 
the comment meta data is provided to allow the simulator to initialize 
state on the fly.

------------------------------------
### aluOp

#### CBP
```
No Operand
[PC: 0x80002af0 type: aluOp ] 

One Operand 
[PC: 0x80002aec type: aluOp 1st input:  (int: 1, idx: 8 val: deadbeef)  ]

Two Operand 
[PC: 0x3ba768 type: aluOp 1st input:  (int: 1, idx: 10 val: deadbeef)  output:  (int: 1, idx: 64 val: 6)  ]

Three Operand
[PC: 0x41df08 type: aluOp 1st input:  (int: 1, idx: 8 val: deadbeef) 2nd input:  (int: 1, idx: 19 val: deadbeef)  output:  (int: 1, idx: 64 val: 6)  ]

Four Operand
[PC: 0x8000055c type: aluOp 1st input:  (int: 1, idx: 64 val: deadbeef) 2nd input:  (int: 1, idx: 0 val: deadbeef) 3rd input:  (int: 1, idx: 1 val: deadbeef)  output:  (int: 1, idx: 0 val: ffffff8ac1fe979a)  ]
```

#### ASM

All aluOps are translated to real RISC-V instructions based on the 
operand count.  RISC-V has no real instructions with only one operand, 
instead the two operand form is used, with x1 as RD

Any operands which exceed their encoding range are capped at the maximum 
value for that range. 
```
RD:64  is converted to x31.
```

Any RD:0 is converted to x1 to avoid NOP optimizations in some models.

Note that in no case is a register pre-initialized to the value shown in the 
meta data. This would insert additional instructions into the trace. Instead 
the comment meta data is provided to allow the simulator to initialize 
state on the fly.

```
fence.i          //PC:80002af0
add x1,x8        //PC:80002aec                           R1:8  V:deadbeef
add x31,x10      //PC:3ba768   RD:64 V:6                 R1:10 V:deadbeef
add x31,x8,x19   //PC:41df08   RD:64 V:6                 R1:8  V:deadbeef R2:19 V:deadbeef
fsl x31,x2,x3,x4 //PC:8000055c RD:0  V:ffffff8ac1fe979a  R1:64 V:deadbeef R2:0  V:deadbeef R3:1 V:deadbeef
```
------------------------------------
### callDirBrOp
#### CBP
```
[PC: 0x80000540 type: callDirBrOp ( tkn:1 tar: 0x800019e4)  output:  (int: 1, idx: 30 val: 80000544)  ]
```
#### ASM
This type is converted to a JAL x0, \<offset\> instruction. 

In the example the current PC is 0x80000540, the target is 0x800019e4 if taken, so the offset is calculated as 0x800019e4 - 0x80000540 giving 0x14A4.
```
jal x30, 0x14A4     //PC:80000540 TAR:800019e4 OFF:14A4 TKN=1
```

------------------------------------
### callIndBrOp
#### CBP
```
[PC: 0x41dbfc type: callIndBrOp ( tkn:1 tar: 0x3bcc18)  1st input:  (int: 1, idx: 8 val: deadbeef)  output:  (int: 1, idx: 30 val: 41dc00)  ]
```
#### ASM
This is translated as shown, note there is no offset. In a simulator where 
register state is initialized on the fly X8 in this case would be 
initialized to the target address. And of course x30 would be written with 
the return address.
```
jalr x30, x8, 0 //PC:41dbfc TAR:3bcc18 OFF:0x0 TKN:1 RD:30 V:41dc00 R1:8 V:deadbeef
```

------------------------------------
### condBrOp
CBP
```
TAKEN
[PC: 0x40e530 type: condBrOp ( tkn:1 tar: 0x40e538)  1st input:  (int: 1, idx: 0 val: deadbeef)  ]

NOT TAKEN
[PC: 0x3bd3cc type: condBrOp ( tkn:0 tar: 0x3bd3d0)  1st input:  (int: 1, idx: 26 val: deadbeef)  ]
```

The taken/not-taken behaviors are created using BEQ and BNE and knowing that X0 will always be zero.

For the taken case the offset is calculated and provided in the meta data as well as the instruction
declaration.  If the offset is greater than the encoding field width a warning is emitted and the asm 
line has TOO_LRG_OFF appended.

Zero offset is used for the not-taken case.

A taken conditional branch:
```
BEQ x0,x0,0x8  //PC:40e530 TAR:40e538 OFF:8 TKN:1 R1:0 V:deadbeef
```

A not-taken conditional branch is emitted as:
```
BNE x0,x0,0  //PC:0x3bd3cc TAR:0x3bd3d0 OFF:0 TKN:0 R1:26 V:deadbeef
```

------------------------------------
### fpOp
CBP
```
These have not been seen in any traces so far
```
------------------------------------
### loadOp

#### CBP
```
[PC: 0x3b7604 type: loadOp ea: 0x895b13 size: 1 1st input:  (int: 1, idx: 8 val: deadbeef)  output:  (int: 1, idx: 9 val: 0)  ]

[PC: 0x3ba764 type: loadOp ea: 0x895a30 size: 2 1st input:  (int: 1, idx: 9 val: deadbeef)  output:  (int: 1, idx: 9 val: 4630)  ]

[PC: 0x3b74fc type: loadOp ea: 0x554070 size: 4 1st input:  (int: 1, idx: 8 val: deadbeef)  output:  (int: 1, idx: 8 val: 2)  ]

[PC: 0x80002af8 type: loadOp ea: 0x800085d0 size: 8 1st input:  (int: 1, idx: 31 val: deadbeef)  output:  (int: 1, idx: 30 val: 80002b38)  ]
```

#### ASM

```
lbu  x0, 0(x0) // PC:3b7604   EA:895b13   SZ:1 RD:9  V:0        R1:8  V:deadbeef 
lhu  x0, 0(x0) // PC:3ba764   EA:895a30   SZ:2 RD:9  V:4630     R1:9  V:deadbeef
lwu  x0, 0(x0) // PC:3b74fc   EA:554070   SZ:4 RD:8  V:2        R1:8  V:deadbeef
ld   x0, 0(x0) // PC:80002af8 EA:800085d0 SZ:8 RD:30 V:80002b38 R1:31 V:deadbeef
```
------------------------------------
### retBrOp

#### CBP
```
[PC: 0x40c690 type: retBrOp ( tkn:1 tar: 0x3bd028)  1st input:  (int: 1, idx: 30 val: deadbeef)  ]
```

#### ASM
```
jalr x0, x1, 0 // PC:40c690 TAR:3bd028 R1:30 V:deadbeef
```

------------------------------------
### slowAluOp

#### CBP
```
[PC: 0x3b8094 type: slowAluOp 1st input:  (int: 1, idx: 8 val: deadbeef) 2nd input:  (int: 1, idx: 11 val: deadbeef) 3rd input:  (int: 1, idx: 10 val: deadbeef)  output:  (int: 1, idx: 8 val: 5555a8)  ]
```


#### ASM
Assembly for these types uses the divide instruction.  The expectation is
that this form will not create an exception. TODO: verify this.

```
divu x0,x0,x0  //PC:0x3b8094              
```

------------------------------------
### stOp

#### CBP

```
[PC: 0x3b74dc type: stOp ea: 0x54d909 size: 1 1st input:  (int: 1, idx: 10 val: deadbeef) 2nd input:  (int: 1, idx: 65 val: deadbeef)  ]
<No example available reusing size 1>
[PC: 0x3b74dc type: stOp ea: 0x54d909 size: 2 1st input:  (int: 1, idx: 10 val: deadbeef) 2nd input:  (int: 1, idx: 13 val: deadbeef)  ]
[PC: 0x3aabe0 type: stOp ea: 0x554070 size: 4 1st input:  (int: 1, idx: 8 val: deadbeef) 2nd input:  (int: 1, idx: 12 val: deadbeef)  ]
[PC: 0x3ba808 type: stOp ea: 0x8934f8 size: 8 1st input:  (int: 1, idx: 10 val: deadbeef) 2nd input:  (int: 1, idx: 8 val: deadbeef)  ]

```

#### ASM

```
sb x31,0(x10) // PC:3b74dc EA:54d909 SIZE:1 R1:10 V:deadbeef R2:65 V:deadbeef  
sh x13,0(x10) // PC:3b74dc EA:54d909 SIZE:2 R1:10 V:deadbeef R2:13 V:deadbeef  
sw x12,0(x8)  // PC:3aabe0 EA:554070 SIZE:4 R1:8  V:deadbeef R2:12 V:deadbeef 
sd rs2,0(rs1) // PC:3ba808 EA:8934f8 SIZE:8 R1:10 V:deadbeef R2:8  V:deadbeef
```
 
------------------------------------
### uncondDirBrOp
CBP
```
[PC: 0x41defc type: uncondDirBrOp ( tkn:1 tar: 0x41df10)  ]
```
ASM
JAL with X0 as rd is used, the offset is calculated from the meta data. If the offset overflows a warning is issued and the offset is set 
to 0x0. In this example 0x41df10 - 0x41defc = 0x14.
```
jal x0,0x14 //PC:41defc TAR:41df10 OFF:14 TKN:1
```

------------------------------------
### uncondIndBrOp
CBP
```
[PC: 0x41df84 type: uncondIndBrOp ( tkn:1 tar: 0x41df00)  1st input:  (int: 1, idx: 11 val: deadbeef)  ]
```

ASM
JALR with X0 as rd is used, the offset is calculated from the meta data. If the offset overflows a warning is issued and the offset is set 
to 0x0. In this example 0x41df00 - 0x41df84 = 0xF7C. (-132 decimal)
```
jalr x0,x11,0xf7c //PC:41df84 TAR:41df00 OFF:f7c TKN:1
```
