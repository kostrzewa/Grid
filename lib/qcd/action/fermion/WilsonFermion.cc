#include <Grid.h>

namespace Grid {
namespace QCD {

  const std::vector<int> WilsonFermionStatic::directions   ({0,1,2,3, 0, 1, 2, 3});
  const std::vector<int> WilsonFermionStatic::displacements({1,1,1,1,-1,-1,-1,-1});
  int WilsonFermionStatic::HandOptDslash;

  /////////////////////////////////
  // Constructor and gauge import
  /////////////////////////////////

  template<class Impl>
  WilsonFermion<Impl>::WilsonFermion(GaugeField &_Umu,
				     GridCartesian         &Fgrid,
				     GridRedBlackCartesian &Hgrid, 
				     RealD _mass,const ImplParams &p) :
        Kernels(p),
        _grid(&Fgrid),
	_cbgrid(&Hgrid),
	Stencil    (&Fgrid,npoint,Even,directions,displacements),
	StencilEven(&Hgrid,npoint,Even,directions,displacements), // source is Even
	StencilOdd (&Hgrid,npoint,Odd ,directions,displacements), // source is Odd
	mass(_mass),
	Umu(&Fgrid),
	UmuEven(&Hgrid),
	UmuOdd (&Hgrid) 
  {
    // Allocate the required comms buffer
    comm_buf.resize(Stencil._unified_buffer_size); // this is always big enough to contain EO
    ImportGauge(_Umu);
  }

  template<class Impl>
  void WilsonFermion<Impl>::ImportGauge(const GaugeField &_Umu)
  {
    Impl::DoubleStore(GaugeGrid(),Umu,_Umu);
    pickCheckerboard(Even,UmuEven,Umu);
    pickCheckerboard(Odd ,UmuOdd,Umu);
  }
  
  /////////////////////////////
  // Implement the interface
  /////////////////////////////
      
  template<class Impl>
  RealD WilsonFermion<Impl>::M(const FermionField &in, FermionField &out) 
  {
    out.checkerboard=in.checkerboard;
    Dhop(in,out,DaggerNo);
    return axpy_norm(out,4+mass,in,out);
  }

  template<class Impl>
  RealD WilsonFermion<Impl>::Mdag(const FermionField &in, FermionField &out) 
  {
    out.checkerboard=in.checkerboard;
    Dhop(in,out,DaggerYes);
    return axpy_norm(out,4+mass,in,out);
  }

  template<class Impl>
  void WilsonFermion<Impl>::Meooe(const FermionField &in, FermionField &out) 
  {
    if ( in.checkerboard == Odd ) {
      DhopEO(in,out,DaggerNo);
    } else {
      DhopOE(in,out,DaggerNo);
    }
  }
  template<class Impl>
  void WilsonFermion<Impl>::MeooeDag(const FermionField &in, FermionField &out) 
  {
    if ( in.checkerboard == Odd ) {
      DhopEO(in,out,DaggerYes);
    } else {
      DhopOE(in,out,DaggerYes);
    }
  }

  template<class Impl>
  void WilsonFermion<Impl>::Mooee(const FermionField &in, FermionField &out) {
    out.checkerboard = in.checkerboard;
    typename FermionField::scalar_type scal(4.0+mass);
    out = scal*in;
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::MooeeDag(const FermionField &in, FermionField &out) {
    out.checkerboard = in.checkerboard;
    Mooee(in,out);
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::MooeeInv(const FermionField &in, FermionField &out) {
    out.checkerboard = in.checkerboard;
    out = (1.0/(4.0+mass))*in;
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::MooeeInvDag(const FermionField &in, FermionField &out) {
    out.checkerboard = in.checkerboard;
    MooeeInv(in,out);
  }
  
  ///////////////////////////////////
  // Internal
  ///////////////////////////////////

  template<class Impl>
  void WilsonFermion<Impl>::DerivInternal(StencilImpl & st,
					  DoubledGaugeField & U,
					  GaugeField &mat,
					  const FermionField &A,
					  const FermionField &B,int dag) {
	
    assert((dag==DaggerNo) ||(dag==DaggerYes));
    
    Compressor compressor(dag);
    
    FermionField Btilde(B._grid);
    FermionField Atilde(B._grid);
    Atilde = A;

    st.HaloExchange(B,comm_buf,compressor);
    
    for(int mu=0;mu<Nd;mu++){
      
      ////////////////////////////////////////////////////////////////////////
      // Flip gamma (1+g)<->(1-g) if dag
      ////////////////////////////////////////////////////////////////////////
      int gamma = mu;
      if ( !dag ) gamma+= Nd;
      
      ////////////////////////
      // Call the single hop
      ////////////////////////
PARALLEL_FOR_LOOP
	for(int sss=0;sss<B._grid->oSites();sss++){
	  Kernels::DiracOptDhopDir(st,U,comm_buf,sss,sss,B,Btilde,mu,gamma);
	}
      
      //////////////////////////////////////////////////
      // spin trace outer product
      //////////////////////////////////////////////////
      Impl::InsertForce4D(mat,Btilde,Atilde,mu);

    }
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::DhopDeriv(GaugeField &mat,const FermionField &U,const FermionField &V,int dag)
  {
    conformable(U._grid,_grid);  
    conformable(U._grid,V._grid);
    conformable(U._grid,mat._grid);
    
    mat.checkerboard = U.checkerboard;
    
    DerivInternal(Stencil,Umu,mat,U,V,dag);
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::DhopDerivOE(GaugeField &mat,const FermionField &U,const FermionField &V,int dag)
  {
    conformable(U._grid,_cbgrid);  
    conformable(U._grid,V._grid);
    conformable(U._grid,mat._grid);
    
    assert(V.checkerboard==Even);
    assert(U.checkerboard==Odd);
    mat.checkerboard = Odd;
    
    DerivInternal(StencilEven,UmuOdd,mat,U,V,dag);
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::DhopDerivEO(GaugeField &mat,const FermionField &U,const FermionField &V,int dag)
  {
    conformable(U._grid,_cbgrid);  
    conformable(U._grid,V._grid);
    conformable(U._grid,mat._grid);
	
    assert(V.checkerboard==Odd);
    assert(U.checkerboard==Even);
    mat.checkerboard = Even;
	
    DerivInternal(StencilOdd,UmuEven,mat,U,V,dag);
  }
  

  template<class Impl>
  void WilsonFermion<Impl>::Dhop(const FermionField &in, FermionField &out,int dag) {
    conformable(in._grid,_grid); // verifies full grid
    conformable(in._grid,out._grid);
    
    out.checkerboard = in.checkerboard;
    
    DhopInternal(Stencil,Umu,in,out,dag);
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::DhopOE(const FermionField &in, FermionField &out,int dag) {
    conformable(in._grid,_cbgrid);    // verifies half grid
    conformable(in._grid,out._grid); // drops the cb check
    
    assert(in.checkerboard==Even);
    out.checkerboard = Odd;
    
    DhopInternal(StencilEven,UmuOdd,in,out,dag);
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::DhopEO(const FermionField &in, FermionField &out,int dag) {
    conformable(in._grid,_cbgrid);    // verifies half grid
    conformable(in._grid,out._grid); // drops the cb check
    
    assert(in.checkerboard==Odd);
    out.checkerboard = Even;
    
    DhopInternal(StencilOdd,UmuEven,in,out,dag);
  }
  
  template<class Impl>
  void WilsonFermion<Impl>::Mdir (const FermionField &in, FermionField &out,int dir,int disp) {
    DhopDir(in,out,dir,disp);
  }
  

  template<class Impl>
  void WilsonFermion<Impl>::DhopDir(const FermionField &in, FermionField &out,int dir,int disp){
    
    int skip = (disp==1) ? 0 : 1;
    int dirdisp  = dir+skip*4;
    int gamma    = dir+(1-skip)*4;
    
    DhopDirDisp(in,out,dirdisp,gamma,DaggerNo);
    
  };
  
  template<class Impl>
  void WilsonFermion<Impl>::DhopDirDisp(const FermionField &in, FermionField &out,int dirdisp,int gamma,int dag) {
    
    Compressor compressor(dag);
    
    Stencil.HaloExchange(in,comm_buf,compressor);
    
PARALLEL_FOR_LOOP
      for(int sss=0;sss<in._grid->oSites();sss++){
	Kernels::DiracOptDhopDir(Stencil,Umu,comm_buf,sss,sss,in,out,dirdisp,gamma);
      }
    
  };


  template<class Impl>
  void WilsonFermion<Impl>::DhopInternal(StencilImpl & st,DoubledGaugeField & U,
					 const FermionField &in, FermionField &out,int dag) {

    assert((dag==DaggerNo) ||(dag==DaggerYes));

    Compressor compressor(dag);
    st.HaloExchange(in,comm_buf,compressor);
    
    if ( dag == DaggerYes ) {
      if( HandOptDslash ) {
PARALLEL_FOR_LOOP
        for(int sss=0;sss<in._grid->oSites();sss++){
	  Kernels::DiracOptHandDhopSiteDag(st,U,comm_buf,sss,sss,in,out);
	}
      } else { 
PARALLEL_FOR_LOOP
        for(int sss=0;sss<in._grid->oSites();sss++){
	  Kernels::DiracOptDhopSiteDag(st,U,comm_buf,sss,sss,in,out);
	}
      }
    } else {
      if( HandOptDslash ) {
PARALLEL_FOR_LOOP
        for(int sss=0;sss<in._grid->oSites();sss++){
	  Kernels::DiracOptHandDhopSite(st,U,comm_buf,sss,sss,in,out);
	}
      } else { 
PARALLEL_FOR_LOOP
        for(int sss=0;sss<in._grid->oSites();sss++){
	  Kernels::DiracOptDhopSite(st,U,comm_buf,sss,sss,in,out);
	}
      }
    }
  };
 
  FermOpTemplateInstantiate(WilsonFermion);


}}



