# nanopb runtime is vendored in nanopb_runtime/ — no component registry needed.
# lot.pb.c / lot.pb.h are pre-generated from lot.proto (nanopb 0.4.9.1).
# To regenerate: python3 -m nanopb.generator.nanopb_generator lot.proto

idf_component_register(
    SRCS
        "lot_proto.c"
        "lot.pb.c"
        "nanopb_runtime/pb_common.c"
        "nanopb_runtime/pb_encode.c"
        "nanopb_runtime/pb_decode.c"
    INCLUDE_DIRS
        "include"
        "."                      # lot.pb.h lives here
        "nanopb_runtime"         # pb.h, pb_encode.h, pb_decode.h, pb_common.h
)
