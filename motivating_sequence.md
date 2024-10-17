# Advantage of using PySequence internally.

CPython provides an API to handle any type of
sequence by the same interface:

* PySequence_Check
* PySequence_GetItem

or similar methods.

Currenty the main effect on user side is that one does not need to call
`list(obj)` when returning an object. Its main use is for array as
a large array does not be converted to a list before passing it
as output.


```
# record for reference waveform
record(pycalc, "ref_wf")
{
   field(DESC, "reference waveform")
   field(CALC, "( A * np.sin(B * (C + np.asarray(D)) ) ).tolist()")
   #field(CALC, "test_pydev.calc(%A%, %B%, %C%, %D%, tpro=%TPRO%) ")
   field(INPA, "src:amp")
   field(INPB, "src:frq")
   field(INPC, "src:phs")
   field(INPD, "src:t")
   field(FTA, "DOUBLE")
   field(FTB, "DOUBLE")
   field(FTC, "DOUBLE")
   field(FTD, "DOUBLE")
   field(MED, 64)
   field(MEVL, 64)
   field(FTVL, "DOUBLE")
   field(TPRO, 1)
}

record(waveform, "src:t")
{
    field(DTYP, "pydev")
    field(INP, "@np.linspace(0, 2*np.pi, num=64 + 1).tolist()")
    field(FTVL, "DOUBLE")
    field(NELM, 64)
    field(PINI, "YES")
    field(FLNK, "ref_wf")
}

record(ao, "src:amp")
{
    field(VAL, 1.0)
    field(FLNK, "ref_wf")
}

record(ao, "src:frq")
{
    field(VAL, 2.0)
    field(FLNK, "ref_wf")
}

record(ao, "src:phs")
{
    field(VAL, 0)
    field(FLNK, "ref_wf")
}



```

```python

from typing import Sequence
import numpy as np



```