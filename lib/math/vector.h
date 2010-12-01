/*
   Copyright 2008 Brain Research Institute, Melbourne, Australia

   Written by J-Donald Tournier, 19/05/09.

   This file is part of MRtrix.

   MRtrix is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   MRtrix is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __math_vector_h__
#define __math_vector_h__

#include <fstream>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_vector_float.h>

#include "mrtrix.h"
#include "math/math.h"

#define LOOP(op) for (size_t i = 0; i < size(); i++) { op; }

namespace MR {
  namespace Math {
    
    //! \cond skip

    template <typename T> class GSLVector;
    template <> class GSLVector <float> : public gsl_vector_float { public: void set (float* p) { data = p; } };
    template <> class GSLVector <double> : public gsl_vector { public: void set (double* p) { data = p; } };

    template <typename T> class GSLBlock;

    template <> class GSLBlock <float> : public gsl_block_float 
    {
      public: 
	static gsl_block_float* alloc (size_t n) { return (gsl_block_float_alloc (n)); }
	static void free (gsl_block_float* p) { gsl_block_float_free (p); }
    };

    template <> class GSLBlock <double> : public gsl_block 
    { 
      public: 
	static gsl_block* alloc (size_t n) { return (gsl_block_alloc (n)); }
	static void free (gsl_block* p) { gsl_block_free (p); }
    };

    template <typename U> class Matrix;
    //! \endcond




    /** @defgroup linalg Linear Algebra
      Provides classes and function to perform basic linear algebra operations.
      The main classes are the Vector and Matrix class, and their associated views.

      @{ */

    //! provides access to data as a vector
    /** \note this class is not capable of managing its own data allocation. 
      See the Vector class for a more general interface. */
    template <typename T> class Vector : protected GSLVector<T>
    {
      public:
        template <typename U> friend class Vector;
        typedef T value_type;

        //! A class to reference existing Vector data
        /*! This class is used purely to access and modify the elements of
         * existing Vectors. It cannot perform allocation/deallocation or
         * resizing operations. It is designed to be returned by members
         * functions of the Vector and Matrix classes to allow convenient
         * access to specific portions of the data (e.g. a row of a Matrix,
         * etc.). */
        class View : public Vector<T> {
          public:

            //! assign the specified \a value to all elements of the vector
            View& operator= (T value) throw () 
            { 
              LOOP (operator[](i) = value);
              return (*this);
            }   

            //! assign the values in \a V to the corresponding elements of the vector
            View& operator= (const Vector<T>& V) { 
              assert (size() == V.size()); 
              LOOP (operator[](i) = V[i]); 
              return (*this);
            }

            //! assign the values in \a V to the corresponding elements of the vector
            template <typename U> View& operator= (const Vector<U>& V) 
            { 
              assert (size() == V.size()); 
              LOOP (operator[](i) = V[i]); 
              return (*this);
            }

          private:
            View (T* vector_data, size_t nelements, size_t skip = 1) throw ()
            { 
              GSLVector<T>::size = nelements;
              GSLVector<T>::stride = skip;
              GSLVector<T>::set (vector_data);
              GSLVector<T>::block = NULL;
              GSLVector<T>::owner = 0;
            }

            friend class Vector<T>;
            friend class Matrix<T>;
        };

        //! construct empty vector
        Vector () throw () 
        {
          GSLVector<T>::size = GSLVector<T>::stride = 1; 
          data = NULL; 
          block = NULL;
          owner = 1;
        }

        //! copy constructor
        Vector (const Vector& V)
        {
          initialize (V.size());
          LOOP (operator[](i) = V[i]); 
        }

        //! copy constructor
        template <typename U> Vector (const Vector<U>& V)
        {
          initialize (V.size());
          LOOP (operator[](i) = V[i]); 
        }

        //! construct vector of size \a nelements
	/** \note the elements of the vector are left uninitialised. */
        Vector (size_t nelements) { initialize (nelements); }

	//! construct from existing data array
        Vector (T* vector_data, size_t nelements, size_t skip = 1) throw ()
        { 
	  GSLVector<T>::size = nelements;
	  GSLVector<T>::stride = skip;
	  set (vector_data);
	  block = NULL;
	  owner = 0;
        }

	//! construct a vector by reading from the text file \a filename
        Vector (const std::string& file)
        { 
          GSLVector<T>::size = GSLVector<T>::stride = 1; 
          data = NULL; 
          block = NULL;
          owner = 1;
          load (file); 
        } 

	//! destructor
        ~Vector ()
        {
          if (block) { 
            assert (owner);
            GSLBlock<T>::free (block);
          }
        }

	//! deallocate the vector data
        Vector& clear () 
        {
          if (block) {
            assert (owner);
            GSLBlock<T>::free (block);
          }
	  GSLVector<T>::size = GSLVector<T>::stride = 0; 
          data = NULL; 
          block = NULL; 
          owner = 1; 
          return (*this); 
        }

	//! allocate the vector to have the same size as \a V
        Vector& allocate (const Vector& V) { return (allocate (V.size())); } 

	//! allocate the vector to have the same size as \a V
        template <typename U> Vector& allocate (const Vector<U>& V) { return (allocate (V.size())); }

	//! allocate the vector to have size \a nelements
        Vector& allocate (size_t nelements)
        {
          if (nelements == size()) return (*this);
          if (block) {
            assert (owner);
            if (block->size < nelements) {
	      GSLBlock<T>::free (block);
	      block = NULL;
            }
          }
          if (!block && nelements) {
            block = GSLBlock<T>::alloc (nelements);
            if (!block)
              throw Exception ("Failed to allocate memory for Vector data");
          }
	  GSLVector<T>::size = nelements; 
          GSLVector<T>::stride = 1;
          owner = 1;
          data = block ? block->data : NULL;
          return (*this);
        }

	//! resize the vector to have size \a nelements, preserving existing data
        /*! The \c fill_value argument determines what value the elements of
         * the Vector will be set to in case the size requested exceeds the
         * current size. */
        Vector& resize (size_t nelements, value_type fill_value = 0.0) 
        {
          if (nelements == 0) {
            GSLVector<T>::size = 0;
            return (*this);
          }
          if (!block) {
            allocate (nelements);
            operator= (fill_value);
            return (*this);
          }
          if (nelements*stride() > block->size) {
            Vector V (nelements);
            V.sub (0, size()) = *this;
            V.sub (size(), V.size()) = fill_value;
            swap (V);
            return (*this);
          }
          GSLVector<T>::size = nelements; 
          return (*this); 
        }

	//! read vector data from the text file \a filename
        Vector& load (const std::string& filename) 
        { 
          std::ifstream in (filename.c_str()); 
          if (!in) 
            throw Exception ("cannot open matrix file \"" + filename + "\": " + strerror (errno)); 
          try { 
            in >> *this;
          }
          catch (Exception& E) { 
            throw Exception (E, "error loading matrix file \"" + filename + "\"");
          }
          return (*this);
        }

	//! write to text file \a filename
        void save (const std::string& filename) const
        {   
          std::ofstream out (filename.c_str()); 
          if (!out)
            throw Exception ("cannot open matrix file \"" + filename + "\": " + strerror (errno)); 
          out << *this; 
        }

	//! used to obtain a pointer to the underlying GSL structure
	GSLVector<T>* gsl () { return (this); }
	//! used to obtain a pointer to the underlying GSL structure
        const GSLVector<T>* gsl () const { return (this); }

	//! true if vector points to existing data
        bool is_set () const throw () { return (ptr()); }
	//! returns number of elements of vector
        size_t size () const throw ()  { return (GSLVector<T>::size); }

	//! returns a reference to the element at \a i
        T& operator[] (size_t i) throw ()          { return (ptr()[i*stride()]); }

	//! returns a reference to the element at \a i
        const T& operator[] (size_t i) const throw ()    { return (ptr()[i*stride()]); }

	//! return a pointer to the underlying data
        T* ptr () throw () { return ((T*) (data)); }

	//! return a pointer to the underlying data
        const T* ptr () const throw () { return ((const T*) (data)); }

	//! return the stride of the vector
        size_t stride () const throw () { return (GSLVector<T>::stride); }

	//! assign the specified \a value to all elements of the vector
        Vector& operator= (T value) throw () 
        { 
          LOOP (operator[](i) = value);
          return (*this);
        }   

	//! assign the values in \a V to the corresponding elements of the vector
        Vector& operator= (const Vector& V) 
        { 
          allocate (V);
          LOOP (operator[](i) = V[i]); 
          return (*this);
        }

	//! assign the values in \a V to the corresponding elements of the vector
        template <typename U> Vector& operator= (const Vector<U>& V)
        { 
          allocate (V);
          LOOP (operator[](i) = V[i]); 
          return (*this);
        }


	//! set all elements of vector to zero
        Vector& zero () throw () { LOOP (operator[](i) = 0.0); return (*this); }

	//! swap contents with \a V without copying
        void swap (Vector& V) throw () 
        { 
          char c [sizeof (Vector)];
          memcpy (&c, this, sizeof (Vector));
          memcpy (this, &V, sizeof (Vector));
          memcpy (&V, &c, sizeof (Vector));
        } 

	//! add \a value to all elements of the vector
        Vector& operator+= (T value) throw () { LOOP (operator[](i) += value); return (*this); } 
	//! subtract \a value from all elements of the vector
        Vector& operator-= (T value) throw () { LOOP (operator[](i) -= value); return (*this); }   
	//! multiply all elements of the vector by \a value
        Vector& operator*= (T value) throw () { LOOP (operator[](i) *= value); return (*this); }   
	//! divide all elements of the vector by \a value
        Vector& operator/= (T value) throw () { LOOP (operator[](i) /= value); return (*this); }   

	//! add each element of \a V to the corresponding element of the vector
        Vector& operator+= (const Vector& V) throw () { LOOP (operator[](i) += V[i]); return (*this); }   
	//! subtract each element of \a V from the corresponding element of the vector
        Vector& operator-= (const Vector& V) throw () { LOOP (operator[](i) -= V[i]); return (*this); }
	//! multiply each element of \a V by the corresponding element of the vector
        Vector& operator*= (const Vector& V) throw () { LOOP (operator[](i) *= V[i]); return (*this); }
	//! divide each element of \a V by the corresponding element of the vector
        Vector& operator/= (const Vector& V) throw () { LOOP (operator[](i) /= V[i]); return (*this); }

	//! return a subvector of the vector
        View sub (size_t from, size_t to) throw () 
        { 
          assert (from <= to && to <= size());
          return (View (ptr() + from*stride(), to-from, stride()));
        }

	//! return a subvector of the vector
        const View sub (size_t from, size_t to) const throw ()
        { 
          assert (from <= to && to <= size());
          return (View (const_cast<T*> (ptr()) + from*stride(), to-from, stride()));
        }

	//! return a subvector of the vector
        View sub (size_t from, size_t to, size_t skip) throw () 
        { 
          assert (from <= to && to <= size());
          return (View (ptr() + from*stride(), ceil<size_t> ((to-from)/float(skip)), stride()*skip));
        }

	//! return a subvector of the vector
        const View sub (size_t from, size_t to, size_t skip) const throw ()
        { 
          assert (from <= to && to <= size());
          return (View (ptr() + from*stride(), ceil<size_t> ((to-from)/float(skip)), stride()*skip));
        }

	//! write the vector \a V to \a stream as text
        friend std::ostream& operator<< (std::ostream& stream, const Vector& V)
        {
          for (size_t i = 0; i < V.size(); i++) stream << V[i] << " "; 
          return (stream); 
        }

	//! read the vector data from \a stream and assign to the vector \a V
        friend std::istream& operator>> (std::istream& stream, Vector& V)
        {
          std::vector<T> vec;
          while (true) {
            T val;
            stream >> val;
            if (stream.good()) vec.push_back (val);
            else break;
          }

          V.allocate (vec.size());
          for (size_t n = 0; n < V.size(); n++)
            V[n] = vec[n];
          return (stream);
        }

      protected:
        using GSLVector<T>::data;
        using GSLVector<T>::block;
        using GSLVector<T>::owner;

        void initialize (size_t nelements) 
        {   
	  block = GSLBlock<T>::alloc (nelements);
          if (!block) 
            throw Exception ("Failed to allocate memory for Vector data");  
	  GSLVector<T>::size = nelements; 
          GSLVector<T>::stride = 1; 
          data = block->data; 
          owner = 1;
        }
    };



    /** @defgroup vector Vector functions
      @{ */

    //! compute the squared 2-norm of a vector
    template <typename T> inline T norm2 (const T* V, size_t size = 3, size_t stride = 1)
    {
      T n = 0.0; 
      for (size_t i = 0; i < size; i++) n += pow2(V[i*stride]); 
      return (n);
    }

    //! compute the squared 2-norm of a vector
    template <typename T> inline T norm2 (const Vector<T>& V) { return (norm2 (V.ptr(), V.size(), V.stride())); }

    //! compute the 2-norm of a vector
    template <typename T> inline T norm (const T* V, size_t size = 3, size_t stride = 1) { return (sqrt(norm2(V, size, stride))); }

    //! compute the 2-norm of a vector
    template <typename T> inline T norm (const Vector<T>& V) { return (norm(V.ptr(), V.size(), V.stride())); }

    //! compute the squared 2-norm of the difference between two vectors
    template <typename T> inline T norm_diff2 (const T* x, const T* y, size_t size = 3, size_t x_stride = 1, size_t y_stride = 1) 
    {
      T n = 0.0; 
      for (size_t i = 0; i < size; i++) n += pow2(x[i*x_stride] - y[i*y_stride]); 
      return (n);
    }

    //! compute the squared 2-norm of the difference between two vectors
    template <typename T> inline T norm_diff2 (const Vector<T>& x, const Vector<T>& y)
    {
      return (norm_diff2 (x.ptr(), y.ptr(), x.size(), x.stride(), y.stride()));
    }

    //! compute the mean of the elements of a vector
    template <typename T> inline T mean (const T* V, size_t size = 3, size_t stride = 1)
    {
      T n = 0.0; 
      for (size_t i = 0; i < size; i++) 
        n += V[i*stride];
      return (n/size); 
    }

    //! compute the mean of the elements of a vector
    template <typename T> inline T mean (const Vector<T>& V) { return (mean(V.ptr(), V.size(), V.stride())); }

    //! normalise a vector to have unit 2-norm
    template <typename T> inline void normalise (T* V, size_t size = 3, size_t stride = 1)
    {
      T n = norm (V, size, stride);
      for (size_t i = 0; i < size; i++)
        V[i*stride] /= n;
    }

    //! normalise a vector to have unit 2-norm
    template <typename T> inline Vector<T>& normalise (Vector<T>& V)
    {
      normalise (V.ptr(), V.size(), V.stride());
      return (V); 
    }

    //! compute the dot product between two vectors
    template <typename T> inline T dot (const T* x, const T* y, size_t size = 3, size_t x_stride = 1, size_t y_stride = 1) 
    {
      T retval = 0.0;
      for (size_t i = 0; i < size; i++) 
        retval += x[i*x_stride] * y[i*y_stride];
      return (retval);
    }

    //! compute the dot product between two vectors
    template <typename T> inline T dot (const Vector<T>& x, const Vector<T>& y) 
    {
      return (dot (x.ptr(), y.ptr(), x.size(), x.stride(), y.stride())); 
    }

    //! compute the cross product between two vectors
    template <typename T> inline void cross (T* c, const T* x, const T* y, size_t c_stride = 1, size_t x_stride = 1, size_t y_stride = 1) 
    {
      c[0] = x[x_stride]*y[2*y_stride] - x[2*x_stride]*y[y_stride];
      c[c_stride] = x[2*x_stride]*y[0] - x[0]*y[2*y_stride];
      c[2*c_stride] = x[0]*y[y_stride] - x[x_stride]*y[0];
    }

    //! compute the cross product between two vectors
    template <typename T> inline Vector<T>& cross (Vector<T> c, const Vector<T>& x, const Vector<T>& y)
    { 
      cross (c.ptr(), x.ptr(), y.ptr(), c.stride(), x.stride(), y.stride()); 
      return (c); 
    }


    //! find the maximum value of any elements within a vector
    template <typename T> inline T max (const Vector<T>& V, size_t& i) 
    {
      T val (V[0]);
      i = 0;
      for (size_t j = 0; j < V.size(); j++) {
        if (val < V[j]) {
          val = V[j];
          i = j;
        }
      }
      return (val);
    }

    //! find the minimum value of any elements within a vector
    template <typename T> inline T min (const Vector<T>& V, size_t& i) 
    {
      T val (V[0]);
      i = 0;
      for (size_t j = 0; j < V.size(); j++) {
        if (val > V[j]) {
          val = V[j];
          i = j;
        }
      }
      return (val);
    }


    //! find the maximum absolute value of any elements within a vector
    template <typename T> inline T absmax (const Vector<T>& V, size_t& i) 
    {
      T val (abs(V[0]));
      i = 0;
      for (size_t j = 0; j < V.size(); j++) {
        if (val < abs(V[j])) {
          val = abs(V[j]);
          i = j; 
        }
      }
      return (val);
    }

    /** @} */

    /** @} */
  }
}

#undef LOOP

#endif