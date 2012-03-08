/*
    Copyright 2011 Brain Research Institute, Melbourne, Australia

    Written by Robert Smith, 2011.

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



#include "app.h"

#include "image/copy.h"
#include "image/loop.h"
#include "image/header.h"
#include "image/voxel.h"
#include "image/buffer.h"
#include "image/buffer_scratch.h"
#include "image/filter/lcc.h"


using namespace MR;
using namespace App;
using namespace Image;

MRTRIX_APPLICATION

void usage ()
{

  DESCRIPTION
  + "Reads a generated mask image (typically from a thresholded average-DWI image), and outputs a mask containing only the largest structure in the mask (presumably the brain). "
  + "It also fills in any gaps within the brain mask caused by the thresholding process.";

  ARGUMENTS
  + Argument ("input",  "the input mask image") .type_image_in()
  + Argument ("output", "the output mask image").type_image_out();

  OPTIONS
  + Option ("fill", "In addition to extracting the largest connected-component of the mask, also fill in any gaps within this mask");

}



void run ()
{

  Buffer<bool> data_in (argument[0]);
  Buffer<bool>::voxel_type voxel_in (data_in);

  BufferScratch<bool> largest_mask_data (data_in);
  BufferScratch<bool>::voxel_type largest_mask (largest_mask_data);

  {
    Filter::LargestConnectedComponent lcc (voxel_in, "getting largest connected-component...");
    lcc (voxel_in, largest_mask);
  }

  if (get_options ("fill").size()) {

    Loop loop;
    for (loop.start (largest_mask); loop.ok(); loop.next (largest_mask))
      largest_mask.value() = !largest_mask.value();

    BufferScratch<bool> outside_mask_data (data_in);
    BufferScratch<bool>::voxel_type outside_mask (outside_mask_data);

    Filter::LargestConnectedComponent lcc_fill (largest_mask, "filling gaps in mask...");
    lcc_fill (largest_mask, outside_mask);
    for (loop.start (outside_mask, largest_mask); loop.ok(); loop.next (outside_mask, largest_mask))
      largest_mask.value() = !outside_mask.value();

  }

  Buffer<bool> data_out (argument[1], data_in);
  Buffer<bool>::voxel_type voxel_out (data_out);
  copy (largest_mask, voxel_out);

}
