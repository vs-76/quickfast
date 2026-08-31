// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "Profiler.h"
#include <math.h>

using namespace QuickFAST;

ProfileAccumulator * ProfileAccumulator::root_ = 0;

ProfileAccumulator::ProfileAccumulator(const char * name, const char * file, size_t line)
  : name_(name)
  , file_(file)
  , line_(line)
  , entries_(0)
  , exits_(0)
  , pauses_(0)
  , resumes_(0)
  , sum_(0.0)
  , sumOfSquares_(0.0)
  , recursions_(0)
  , recursiveSum_(0.0)
  , recursiveSumOfSquares_(0.0)

{
  next_ = root_;
  root_ = this;
}

void
ProfileAccumulator::write(std::ostream & out)
{
  const ProfileAccumulator * ac = root_;
  out << "name\tfile\tline\tentries\texits\tsum\tsum_of_squares"
    << "\trecursions\trecursive_sum\trecursive_sum_of_squares"
    // helpers
    << "\tnonRsum"
    << "\tnonRmean"
    << std::endl;
  while(ac != 0)
  {
    out << ac->name_
      << '\t' << ac->file_
      << '\t' << ac->line_
      << '\t' << ac->entries_
      << '\t' << ac->exits_
      << '\t' << ac->sum_
      << '\t' << ac->sumOfSquares_
      << '\t' << ac->recursions_
      << '\t' << ac->recursiveSum_
      << '\t' << ac->recursiveSumOfSquares_
      // helpers
      << '\t' << ac->sum_ - ac->recursiveSum_
      << '\t' << (ac->sum_ - ac->recursiveSum_) /double(ac->exits_ - ac->recursions_)
      << std::endl;
    ac = ac->next_;
  }
}

void
ProfileAccumulator::print(std::ostream & out)
{
  const ProfileAccumulator * ac = root_;
  out << "name\tcount\tsum\tmean\tstd_dev\trecursions\trsum\trmean\trstd_dev" << std::endl;
  while(ac != 0)
  {
    double count = double(ac->exits_ - ac->recursions_);
    double sum = ac->sum_ - ac->recursiveSum_;
    double sumsq = ac->sumOfSquares_ - ac->recursiveSumOfSquares_;

    out << ac->name_
      << '\t' << std::fixed << std::setprecision(0) << count
      << '\t' << std::fixed << std::setprecision(0) << sum;
    if(count > 1)
    {
      double mean = sum/ count;
      double stdDev = std::sqrt((sumsq - sum * mean) / (count - 1.0));
      out
        << '\t' << std::fixed << std::setprecision(3) << mean
        << '\t' << std::fixed << std::setprecision(3) << stdDev
        << '\t' << ac->recursions_;
      if(ac->recursions_ > 0)
      {
        double rcount = double(ac->recursions_);
        double rsum = ac->recursiveSum_;
        double rsumsq = ac->recursiveSumOfSquares_;
        double rmean = rsum / rcount;
        double rstdDev = std::sqrt((rsumsq - rsum * rmean) / (rcount - 1.0));
        out << '\t' << std::fixed << std::setprecision(0) << rsum
          << '\t' << std::fixed << std::setprecision(3) << rmean
          << '\t' << std::fixed << std::setprecision(3) << rstdDev;
      }
    }
    out << std::endl;
    ac = ac->next_;
  }
}


