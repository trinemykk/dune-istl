// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_ISTL_FASTAMGSMOOTHER_HH
#define DUNE_ISTL_FASTAMGSMOOTHER_HH

#include <cstddef>

namespace Dune
{
  namespace Amg
  {
    template<int level>
    struct GaussSeidelStepWithDefect
    {
      template<typename M, typename X, typename Y>
      static void forward_apply(const M& A, X& x, Y& d, const Y& b, bool first=false, bool compDef=false)
      {
        typedef typename M::ConstRowIterator RowIterator;
        typedef typename M::ConstColIterator ColIterator;
        typedef typename Y::block_type YBlock;

        typename Y::iterator dIter=d.begin();
        typename Y::const_iterator bIter=b.begin();
        typename X::iterator xIter=x.begin();

        for(RowIterator row=A.begin(), end=A.end(); row != end;
            ++row, ++dIter, ++xIter, ++bIter)
        {
          *dIter = *bIter;

          // do lower triangular matrix
          ColIterator col=(*row).begin();
          for (; col.index()<row.index(); ++col)
            (*col).mmv(x[col.index()],*dIter); // d -= sum_{j<i} a_ij * xnew_j

          ColIterator diag=col;

          // do upper triangular matrix only if not the first iteration
          // because x would be 0 anyway, store result in v for latter use
          YBlock v = 0;
          if (!first)
          {
            //skip diagonal and iterate over the rest
            ColIterator colEnd = row->end();
            for (++col; col != colEnd; ++col)
              (*col).umv(x[col.index()],v);
          }
          *dIter -= v;

          // TODO either go on the next blocklevel recursively or just solve with diagonal (TMP)
          //GaussSeidelStepWithDefect<level-1>::forward_apply(*diag,*xIter,*dIter,*bIter,first,compDef);
          diag->solve(*xIter,*dIter);

          // compute defect if necessary
          if (compDef)
          {
            *dIter = v;

            // Update residual for the symmetric case
            // (uses A_ji=A_ij for memory access efficiency)
            for(col=(*row).begin(); col.index()<row.index(); ++col)
              col->mmv(*xIter, d[col.index()]);     //d_j-=A_ij x_i
          }
        }
        // precious debug code
//         if (compDef)
//         {
//           Y newdef(b);
//           typename Y::iterator dit = newdef.begin();
//           typename X::iterator xit = x.begin();
//           for (RowIterator row = A.begin(); row != A.end(); ++row, ++xit, ++dit)
//             for(ColIterator col = row->begin(); col!=row->end(); ++col)
//               col->mmv(x[col.index()], *dit);
//               //col->mmv(*xit, d[col.index()]);
//           for (int i=0; i<newdef.size(); i++)
//            // std::cout << newdef[i] << " " << d[i] << std::endl;
//             if (std::abs(newdef[i][0]-d[i][0])>1e-4)
//               DUNE_THROW(Dune::Exception,"Falschen Defekt berechnet");
//         }
      }

      template<typename M, typename X, typename Y>
      static void backward_apply(const M& A, X& x, Y& d,
                        const Y& b)
      {
        typedef typename M::ConstRowIterator RowIterator;
        typedef typename M::ConstColIterator ColIterator;

        typename Y::iterator dIter=d.beforeEnd();
        typename X::iterator xIter=x.beforeEnd();
        typename Y::const_iterator bIter=b.beforeEnd();

        for(RowIterator row=A.beforeEnd(), end=A.beforeBegin(); row != end;
            --row, --dIter, --xIter, --bIter)
        {
          *dIter = *bIter;

          // do lower triangular matrix
          ColIterator endCol=(*row).beforeBegin();
          ColIterator col=(*row).beforeEnd();
          for (; col.index()>row.index(); --col)
            (*col).mmv(x[col.index()],*dIter);     // d -= sum_{i>j} a_ij * xnew_j

          ColIterator diag=col;

          // do upper triangular matrix manually skipping diagonal
          for (--col; col!=endCol; --col)
            (*col).mmv(x[col.index()],*dIter);     // d -= sum_{j<i} a_ij * xold_j

          // TODO Not recursive yet. Just solve with the diagonal
          diag->solve(*xIter,*dIter);
        }
      }
    };

    template<>
    struct GaussSeidelStepWithDefect<0>
    {
      template<typename M, typename X, typename Y>
      static void forward_apply(const M& A, X& x, Y& d, const Y& b, bool first, bool compDef)
      {
        //TODO to reproduce the optimized forward version, this needs to be d, but is this generally okay???
        A.solve(x,d);
      }

      template<typename M, typename X, typename Y>
      static void backward_apply(const M& A, X& x, Y& d, const Y& b, bool first, bool compDef)
      {
        //TODO to reproduce the optimized forward version, this needs to be d, but is this generally okay???
        A.solve(x,d);
      }
    };


    struct GaussSeidelPresmoothDefect
    {
      template<typename M, typename X, typename Y>
      static void apply(const M& A, X& x, Y& d,
                        const Y& b, int num_iter)
      {
        // perform iterations. These have to know whether they are first
        // and whether to compute a defect.
        if (num_iter == 1)
          GaussSeidelStepWithDefect<M::blocklevel>::forward_apply(A,x,d,b,true,true);
        else
        {
          GaussSeidelStepWithDefect<M::blocklevel>::forward_apply(A,x,d,b,true,false);
          for (int i=0; i<num_iter-2; i++)
            GaussSeidelStepWithDefect<M::blocklevel>::forward_apply(A,x,d,b, false,false);
          GaussSeidelStepWithDefect<M::blocklevel>::forward_apply(A,x,d,b,false,true);
        }
      }
    };

    struct GaussSeidelPostsmoothDefect {

      template<typename M, typename X, typename Y>
      static void apply(const M& A, X& x, Y& d,
                        const Y& b, int num_iter)
      {
        for (int i=0; i<num_iter; i++)
          GaussSeidelStepWithDefect<M::blocklevel>::backward_apply(A,x,d,b);
      }
    };


    //! JACOBI SMOOTHING

    template<std::size_t level>
    struct JacobiStepWithDefect
    {
      template<typename M, typename X, typename Y>
      static void forward_apply(const M& A, X& x, Y& d, const Y& b, bool first, bool compDef)
      {
        typedef typename M::ConstRowIterator RowIterator;
        typedef typename M::ConstColIterator ColIterator;
        typedef typename Y::block_type YBlock;

        typename Y::iterator dIter=d.begin();
        typename Y::const_iterator bIter=b.begin();
        typename X::iterator xIter=x.begin();

        // a jacobi-intrinsic problem: we need the entire old x until the end of the computation
        const X xold(x);

        for(RowIterator row=A.begin(), end=A.end(); row != end;
            ++row, ++dIter, ++xIter, ++bIter)
        {
          YBlock r = *bIter;

          ColIterator col = row->begin();
          ColIterator colEnd = row->end();
          for (; col.index() < row.index(); ++col)
            col->mmv(xold[col.index()],r);

          ColIterator diag = col;

          for (++col; col != colEnd; ++col)
            col->mmv(xold[col.index()],r);

          //TODO recursion
          diag->solve(*xIter,r);

          if (compDef)
          {
            *dIter = *bIter;
            col = row->begin();
            for (; col != colEnd; ++col)
              col->mmv(x[col.index()],*dIter);
          }
        }
                  // precious debug code
        if (compDef)
        {
          Y newdef(b);
          typename Y::iterator dit = newdef.begin();
          typename X::iterator xit = x.begin();
          for (RowIterator row = A.begin(); row != A.end(); ++row, ++xit, ++dit)
            for(ColIterator col = row->begin(); col!=row->end(); ++col)
              col->mmv(x[col.index()], *dit);
              //col->mmv(*xit, d[col.index()]);
          for (int i=0; i<newdef.size(); i++)
            std::cout << newdef[i] << " " << d[i] << std::endl;
           // if (std::abs(newdef[i][0]-d[i][0])>1e-4)
             // DUNE_THROW(Dune::Exception,"Falschen Defekt berechnet");
        }

      }

      template<typename M, typename X, typename Y>
      static void backward_apply(const M& A, X& x, Y& d, const Y& b)
      {
        typedef typename M::ConstRowIterator RowIterator;
        typedef typename M::ConstColIterator ColIterator;
        typedef typename Y::block_type YBlock;

        typename Y::iterator dIter=d.beforeEnd();
        typename X::iterator xIter=x.beforeEnd();
        typename Y::const_iterator bIter=b.beforeEnd();

        // a jacobi-intrinsic problem: we need the entire old x until the end of the computation
        const X xold(x);

        for(RowIterator row=A.beforeEnd(), end=A.beforeBegin(); row != end;
            --row, --dIter, --xIter, --bIter)
        {
          YBlock r = *bIter;
          ColIterator endCol=(*row).beforeBegin();
          ColIterator col=(*row).beforeEnd();
          for (; col.index()>row.index(); --col)
            (*col).mmv(xold[col.index()],r);     // d -= sum_{i>j} a_ij * xnew_j

          ColIterator diag=col;

          for (--col; col!=endCol; --col)
            (*col).mmv(xold[col.index()],r);     // d -= sum_{j<i} a_ij * xold_j

          diag->solve(*xIter,r);
        }

      }
    };

    struct JacobiPresmoothDefect
    {
      template<typename M, typename X, typename Y>
      static void apply(const M& A, X& x, Y& d, const Y& b, int num_iter)
      {
        for (int i=0; i<num_iter-1; i++)
          JacobiStepWithDefect<M::blocklevel>::forward_apply(A,x,d,b,false,false);
        JacobiStepWithDefect<M::blocklevel>::forward_apply(A,x,d,b,false,true);
      }
    };

    struct JacobiPostsmoothDefect
    {
      template<typename M, typename X, typename Y>
      static void apply(const M& A, X& x, Y& d, const Y& b, int num_iter)
      {
        for (int i=0; i<num_iter; i++)
          JacobiStepWithDefect<M::blocklevel>::backward_apply(A,x,d,b);
      }
    };


  } // end namespace Amg
} // end namespace Dune
#endif
