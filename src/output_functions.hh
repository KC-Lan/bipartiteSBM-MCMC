#ifndef OUTPUT_FUNCTIONS_HH
#define OUTPUT_FUNCTIONS_HH

#include <iostream>
#include "types.hh"

template<typename T>
void output_mat(T mat, std::ostream & stream=std::clog) 
{
  for (size_t r = 0; r < mat.size(); ++ r)
  {
    for (size_t s = 0; s < mat[r].size(); ++s)
    {
      stream << mat[r][s] << " ";
    }
    stream << "\n";
  }
  return;
}
template<typename T>
void output_vec(T vec, std::ostream & stream=std::clog) 
{
  for (auto it = vec.begin(); it != vec.end(); ++it)
  {
    stream << *it << " ";
  }
  stream << "\n";
  return;
}
void output_edge_list(const edge_list_t & edge_list, std::ostream & stream=std::clog);
void output_adj_list(const adj_list_t & adj_list, std::ostream & stream=std::clog);


#endif // OUTPUT_FUNCTIONS_H