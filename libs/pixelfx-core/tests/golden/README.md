# Golden images

Expected output of each operation on the generated test scene (`../fixtures.cpp`), as 16-bit
sRGB PNG, with the settings listed in `../test_pipeline.cpp`. Regenerate only on purpose:

```sh
PFX_GOLDEN_UPDATE=1 build/lib/tests/pfx_tests -tc="golden*"
```

then look at the changed images before committing them.
