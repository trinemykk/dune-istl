AC_DEFUN([DUNE_ISTL_CHECKS],
[
  AC_DEFINE(DUNE_ISTL_FLAVOR_MN, 1, [This is Martin's personal development version of dune-istl!])

  AC_REQUIRE([DUNE_PATH_PARMETIS])
  AC_REQUIRE([DUNE_PATH_SUPERLU])
  AC_REQUIRE([DUNE_PATH_SUPERLU_DIST])
  AC_REQUIRE([DUNE_PARDISO])
  AC_REQUIRE([__AC_FC_NAME_MANGLING])
  AC_REQUIRE([AC_PROG_F77])
  AC_REQUIRE([ACX_BLAS])
  DUNE_BOOST_BASE(, [ DUNE_BOOST_FUSION ] , [] )
  
  # add summary entries for tests not maintained by dune
  DUNE_ADD_SUMMARY_ENTRY([METIS],[$with_metis])
  DUNE_ADD_SUMMARY_ENTRY([BLAS],[$acx_blas_ok])
])

AC_DEFUN([DUNE_ISTL_CHECK_MODULE],
[
    DUNE_CHECK_MODULES([dune-istl], [istl/istlexception.hh])
])
