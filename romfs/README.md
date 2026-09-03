# Local ROMFS files

The source repository does not distribute the eight runtime font and shader files used by
the packaged mod. Put locally obtained copies in these directories before packaging:

```text
romfs/Lib/sead/nvn_font/
romfs/Lib/sead/primitive_renderer/
```

The ready-to-install release archive already contains them. Players do not need to extract
or supply anything.
