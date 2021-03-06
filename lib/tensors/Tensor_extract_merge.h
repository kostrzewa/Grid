#ifndef GRID_EXTRACT_H
#define GRID_EXTRACT_H
/////////////////////////////////////////////////////////////////
// Generic extract/merge/permute
/////////////////////////////////////////////////////////////////

namespace Grid{

////////////////////////////////////////////////////////////////////////////////////////////////
// Extract/merge a fundamental vector type, to pointer array with offset
////////////////////////////////////////////////////////////////////////////////////////////////

template<class vsimd,class scalar>
inline void extract(typename std::enable_if<!isGridTensor<vsimd>::value, const vsimd >::type * y, 
		    std::vector<scalar *> &extracted,int offset){
  // FIXME: bounce off memory is painful
  int Nextr=extracted.size();
  int Nsimd=vsimd::Nsimd();
  int s=Nsimd/Nextr;

  scalar*buf = (scalar *)y;
  for(int i=0;i<Nextr;i++){
    extracted[i][offset] = buf[i*s];
  }
};
////////////////////////////////////////////////////////////////////////
// Merge simd vector from array of scalars to pointer array with offset
////////////////////////////////////////////////////////////////////////
template<class vsimd,class scalar>
inline void merge(typename std::enable_if<!isGridTensor<vsimd>::value, vsimd >::type * y, 
		  std::vector<scalar *> &extracted,int offset){
  int Nextr=extracted.size();
  int Nsimd=vsimd::Nsimd();
  int s=Nsimd/Nextr; // can have sparse occupation of simd vector if simd_layout does not fill it
                     // replicate n-fold. Use to allow Integer masks to 
                     // predicate floating point of various width assignments and maintain conformable.
  scalar *buf =(scalar *) y;
  for(int i=0;i<Nextr;i++){
    for(int ii=0;ii<s;ii++){
      buf[i*s+ii]=extracted[i][offset];
    }
  }
};

////////////////////////////////////////////////////////////////////////////////////////////////
// Extract a fundamental vector type to scalar array 
////////////////////////////////////////////////////////////////////////////////////////////////
template<class vsimd,class scalar>
inline void extract(typename std::enable_if<!isGridTensor<vsimd>::value, const vsimd >::type  &y,std::vector<scalar> &extracted){

  int Nextr=extracted.size();
  int Nsimd=vsimd::Nsimd();
  int s=Nsimd/Nextr;

  scalar *buf = (scalar *)&y;
  for(int i=0;i<Nextr;i++){
    extracted[i]=buf[i*s];
    for(int ii=1;ii<s;ii++){
      if ( buf[i*s]!=buf[i*s+ii] ){
	std::cout<<GridLogMessage << " SIMD extract failure splat = "<<s<<" ii "<<ii<<" " <<Nextr<<" "<< Nsimd<<" "<<std::endl;
	for(int vv=0;vv<Nsimd;vv++) {
	  std::cout<<GridLogMessage<< buf[vv]<<" ";
	}
	std::cout<<GridLogMessage<<std::endl;
	assert(0);
      }
      assert(buf[i*s]==buf[i*s+ii]);
    }
  }

};

////////////////////////////////////////////////////////////////////////
// Merge simd vector from array of scalars
////////////////////////////////////////////////////////////////////////
template<class vsimd,class scalar>
inline void merge(typename std::enable_if<!isGridTensor<vsimd>::value, vsimd >::type  &y,std::vector<scalar> &extracted){
  int Nextr=extracted.size();
  int Nsimd=vsimd::Nsimd();
  int s=Nsimd/Nextr;
  scalar *buf = (scalar *)&y;

  for(int i=0;i<Nextr;i++){
    for(int ii=0;ii<s;ii++){
      buf[i*s+ii]=extracted[i]; // replicates value
    }
  }
};

////////////////////////////////////////////////////////////////////////
// Extract to contiguous array scalar object
////////////////////////////////////////////////////////////////////////
template<class vobj> inline void extract(const vobj &vec,std::vector<typename vobj::scalar_object> &extracted)
{
  typedef typename vobj::scalar_type scalar_type ;
  typedef typename vobj::vector_type vector_type ;

  const int Nsimd=vobj::vector_type::Nsimd();
  int Nextr=extracted.size();
  const int words=sizeof(vobj)/sizeof(vector_type);
  int s=Nsimd/Nextr;

  std::vector<scalar_type *> pointers(Nextr);
  for(int i=0;i<Nextr;i++) 
    pointers[i] =(scalar_type *)& extracted[i];

  vector_type *vp = (vector_type *)&vec;
  for(int w=0;w<words;w++){
    extract<vector_type,scalar_type>(&vp[w],pointers,w);
  }
}
////////////////////////////////////////////////////////////////////////
// Extract to a bunch of scalar object pointers, with offset
////////////////////////////////////////////////////////////////////////
template<class vobj> inline 
void extract(const vobj &vec,std::vector<typename vobj::scalar_object *> &extracted, int offset)
{
  typedef typename vobj::scalar_type scalar_type ;
  typedef typename vobj::vector_type vector_type ;

  const int words=sizeof(vobj)/sizeof(vector_type);
  const int Nsimd=vobj::vector_type::Nsimd();

  int Nextr=extracted.size();
  int s = Nsimd/Nextr;
  scalar_type * vp = (scalar_type *)&vec;

  for(int w=0;w<words;w++){
    for(int i=0;i<Nextr;i++){
      scalar_type * pointer = (scalar_type *)& extracted[i][offset];
      pointer[w] = vp[i*s+w*Nsimd];
    }
  }
}

////////////////////////////////////////////////////////////////////////
// Merge a contiguous array of scalar objects
////////////////////////////////////////////////////////////////////////
template<class vobj> inline 
void merge(vobj &vec,std::vector<typename vobj::scalar_object> &extracted)
{
  typedef typename vobj::scalar_type scalar_type ;
  typedef typename vobj::vector_type vector_type ;
  
  const int Nsimd=vobj::vector_type::Nsimd();
  const int words=sizeof(vobj)/sizeof(vector_type);

  int Nextr = extracted.size();
  int splat=Nsimd/Nextr;

  std::vector<scalar_type *> pointers(Nextr);
  for(int i=0;i<Nextr;i++) 
    pointers[i] =(scalar_type *)& extracted[i];
  
  vector_type *vp = (vector_type *)&vec;
  for(int w=0;w<words;w++){
    merge<vector_type,scalar_type>(&vp[w],pointers,w);
  }
}

////////////////////////////////////////////////////////////////////////
// Merge a bunch of different scalar object pointers, with offset
////////////////////////////////////////////////////////////////////////
template<class vobj> inline 
void merge(vobj &vec,std::vector<typename vobj::scalar_object *> &extracted,int offset)
{
  typedef typename vobj::scalar_type scalar_type ;
  typedef typename vobj::vector_type vector_type ;
  
  const int Nsimd=vobj::vector_type::Nsimd();
  const int words=sizeof(vobj)/sizeof(vector_type);

  int Nextr=extracted.size();
  int s=Nsimd/Nextr;

  scalar_type *pointer;
  scalar_type *vp = (scalar_type *)&vec;

  for(int w=0;w<words;w++){
    for(int i=0;i<Nextr;i++){
      for(int ii=0;ii<s;ii++){
	pointer=(scalar_type *)&extracted[i][offset];
	vp[w*Nsimd+i*s+ii] = pointer[w];
      }
    }
  }
 }
}
#endif
