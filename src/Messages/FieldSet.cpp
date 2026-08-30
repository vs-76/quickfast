// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldSet.h"
#include <Messages/Sequence.h>
#include <Messages/Group.h>
#include <Common/Exceptions.h>
#include <Common/Profiler.h>

#include <algorithm>
#include <functional>
#include <vector>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

// FieldSet manages raw storage and constructs elements in place. reserve()
// would leak the new buffer and strand already-constructed elements if a copy
// threw part way through, and replaceField() destroys a slot before rebuilding
// it, so a throwing constructor would leave a destroyed-but-live element for
// ~FieldSet to destroy a second time. Neither can happen while MessageField
// copies nothing but a reference and a shared_ptr; this pins that down rather
// than leaving it as an unstated assumption.
static_assert(
  std::is_nothrow_copy_constructible<MessageField>::value,
  "FieldSet's in-place storage management assumes MessageField copies cannot throw");
static_assert(
  std::is_nothrow_constructible<MessageField, const FieldIdentity &, const FieldCPtr &>::value,
  "FieldSet::replaceField assumes constructing a MessageField cannot throw");

FieldSet::FieldSet(size_t res)
: fields_(reinterpret_cast<MessageField *>(new unsigned char[sizeof(MessageField) * res]))
, capacity_(res)
, used_(0)
, lookupCursor_(0)
, mayHaveDuplicateIdentities_(false)
, duplicatesKnown_(true)
{
}

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
uint64_t FieldSet::identityCompareCount_ = 0;

void
FieldSet::resetIdentityCompareCount()
{
  identityCompareCount_ = 0;
}

uint64_t
FieldSet::identityCompareCount()
{
  return identityCompareCount_;
}

void
FieldSet::bumpIdentityCompareCount()
{
  ++identityCompareCount_;
}
#endif

void
FieldSet::detectDuplicateIdentities() const
{
  duplicatesKnown_ = true;
  mayHaveDuplicateIdentities_ = false;
  if(used_ < 2)
  {
    return;
  }
  // matches() cannot be true unless the qualified names are equal, so equal
  // names are a superset of the pairs first-match has to worry about. Sorting
  // hashes answers "any duplicates?" in O(F log F) cheap integer compares
  // instead of O(F^2) three-way string compares.
  static thread_local std::vector<size_t> nameHashes;
  nameHashes.clear();
  nameHashes.reserve(used_);
  std::hash<std::string> hasher;
  for(size_t i = 0; i < used_; ++i)
  {
    nameHashes.push_back(hasher(fields_[i].getIdentity().name()));
  }
  std::sort(nameHashes.begin(), nameHashes.end());
  mayHaveDuplicateIdentities_ =
    std::adjacent_find(nameHashes.begin(), nameHashes.end()) != nameHashes.end();
}

size_t
FieldSet::findIndex(const FieldIdentity & identity) const
{
  if(used_ == 0)
  {
    return 0;
  }
  if(!duplicatesKnown_)
  {
    detectDuplicateIdentities();
  }

  const size_t cursor = lookupCursor_;
  if(cursor < used_)
  {
#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
    bumpIdentityCompareCount();
#endif
    if(identity.matches(fields_[cursor].getIdentity()))
    {
      // Only pay for an earlier-duplicate scan when addField has seen one.
      if(mayHaveDuplicateIdentities_)
      {
        for(size_t i = 0; i < cursor; ++i)
        {
#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
          bumpIdentityCompareCount();
#endif
          if(identity.matches(fields_[i].getIdentity()))
          {
            lookupCursor_ = i + 1;
            return i;
          }
        }
      }
      lookupCursor_ = cursor + 1;
      return cursor;
    }
  }

  for(size_t i = 0; i < used_; ++i)
  {
#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
    bumpIdentityCompareCount();
#endif
    if(identity.matches(fields_[i].getIdentity()))
    {
      lookupCursor_ = i + 1;
      return i;
    }
  }
  return used_;
}

FieldSet::~FieldSet()
{
  clear();
  delete [] reinterpret_cast<unsigned char *>(fields_);
}

void
FieldSet::reserve(size_t capacity)
{
  if(capacity > capacity_)
  {
    std::unique_ptr<unsigned char[]> storage(
      new unsigned char[sizeof(MessageField) * capacity]);
    MessageField * buffer = reinterpret_cast<MessageField *>(storage.get());
    for(size_t nField = 0; nField < used_; ++nField)
    {
      new(&buffer[nField]) MessageField(fields_[nField]);
    }
    storage.release();

    MessageField * oldBuffer = fields_;
    size_t oldUsed = used_;
    fields_ = buffer;
    capacity_ = capacity;

    while (oldUsed > 0)
    {
      --oldUsed;
      oldBuffer[oldUsed].~MessageField();
    }
    delete[] reinterpret_cast<unsigned char *>(oldBuffer);
  }
}

void
FieldSet::clear(size_t capacity)
{
  while(used_ > 0)
  {
    --used_;
    fields_[used_].~MessageField();
  }
  lookupCursor_ = 0;
  mayHaveDuplicateIdentities_ = false;
  duplicatesKnown_ = true;
  if(capacity > capacity_)
  {
    reserve(capacity);
  }
}

const MessageField &
FieldSet::operator[](size_t index)const
{
  if(index >= used_)
  {
    throw UsageError("Coding Error", "Accessing FieldSet entry: index out of range.");
  }
  return fields_[index];
}

bool
FieldSet::isPresent(const FieldIdentity & identity) const
{
  const size_t index = findIndex(identity);
  if(index >= used_)
  {
    return false;
  }
  return fields_[index].getField()->isDefined();
}

void
FieldSet::addField(const FieldIdentity & identity, const FieldCPtr & value)
{
  PROFILE_POINT("FieldSet::addField");
  duplicatesKnown_ = false;
  if(used_ >= capacity_)
  {
    PROFILE_POINT("FieldSet::grow");
    reserve(((used_ + 1) * 3) / 2);
  }
  new (fields_ + used_) MessageField(identity, value);
  ++used_;
}

bool
FieldSet::replaceField(const FieldIdentity & identity,
                       const FieldCPtr & value)
{
  const size_t index = findIndex(identity);
  if(index >= used_)
  {
    return false;
  }
  // Falling through on an undefined field used to keep scanning for a later
  // duplicate. First-match says: the field is here but absent → false.
  if(!fields_[index].getField()->isDefined())
  {
    return false;
  }
  (fields_ + index)->~MessageField();  // Explicit destroy
  new (fields_ + index) MessageField(identity, value);
  // The stored identity may differ from the one it replaced.
  duplicatesKnown_ = false;
  return true;
}

bool
FieldSet::getField(const Messages::FieldIdentity & identity, FieldCPtr & value) const
{
  PROFILE_POINT("FieldSet::getField");
  const size_t index = findIndex(identity);
  if(index >= used_)
  {
    return false;
  }
  value = fields_[index].getField();
  return value->isDefined();
}

void
FieldSet::getFieldInfo(size_t index, std::string & name, ValueType::Type & type, FieldCPtr & fieldPtr)const
{
  // operator[], six lines above, has always checked this. Past used_ the slots
  // hold the raw bytes of the unsigned char[] allocation, so the three reads
  // below would dereference a garbage FieldCPtr.
  if(index >= used_)
  {
    throw UsageError("Coding Error", "Accessing FieldSet entry: index out of range.");
  }
  name = fields_[index].name();
  type = fields_[index].getField()->getType();
  fieldPtr = fields_[index].getField();
}

bool
FieldSet::equals (const FieldSet & rhs, std::ostream & reason) const
{
  if(used_ != rhs.used_)
  {
    reason << "Field counts: " << used_ << " != " << rhs.used_;
    return false;
  }
  // application type "any" matches anything.
  if(applicationType_ != "any" && rhs.applicationType_ != "any")
  {
    if(applicationType_ != rhs.applicationType_)
    {
      reason << "Application types: " << applicationType_ << " != " << rhs.applicationType_;
      return false;
    }
    if(!applicationTypeNs_.empty() && !rhs.applicationTypeNs_.empty() && applicationTypeNs_ != rhs.applicationTypeNs_)
    {
      reason << "Application type namespaces: " << applicationTypeNs_ << " != " << rhs.applicationTypeNs_;
      return false;
    }
  }
  for(size_t nField = 0; nField < used_; ++nField)
  {
    if (fields_[nField].name() != rhs.fields_[nField].name())
    {
      reason << "Field[" << nField << "] names: " << fields_[nField].name() << " != " << rhs.fields_[nField].name();
      return false;
    }
    Messages::FieldCPtr f1 = fields_[nField].getField();
    Messages::FieldCPtr f2 = rhs.fields_[nField].getField();
    if(*f1 != *f2)
    {
      reason << "Field[" << nField << "] "<< fields_[nField].name() << "values: " << f1->displayString() << " != " << f2->displayString();
      return false;
    }
  }
  return true;
}


bool
FieldSet::getUnsignedInteger(const FieldIdentity & identity, ValueType::Type type, uint64 & value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = field->toUnsignedInteger();
  }
  return result;
}

bool
FieldSet::getSignedInteger(const FieldIdentity & identity, ValueType::Type type, int64 & value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = field->toSignedInteger();
  }
  return result;
}

bool
FieldSet::getDecimal(const FieldIdentity & identity,ValueType::Type type, Decimal & value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = field->toDecimal();
  }
  return result;
}

bool
FieldSet::getString(const FieldIdentity & identity,ValueType::Type type, const StringBuffer *& value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = & field->toString();
  }

  return result;
}

bool
FieldSet::getGroup(const FieldIdentity & identity, const MessageAccessor *& groupAccessor)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    const GroupCPtr & group = field->toGroup();
    groupAccessor = group.get();
  }
  return result;
}

void
FieldSet::endGroup(const FieldIdentity & identity, const MessageAccessor * groupAccessor)const
{
}

bool
FieldSet::getSequenceLength(
  const FieldIdentity & identity,
  size_t & length)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    const SequenceCPtr & sequence = field->toSequence();
    length = sequence->size();
  }
  return result;
}

bool
FieldSet::getSequenceEntry(const FieldIdentity & identity, size_t index, const MessageAccessor *& entryAccessor)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    const SequenceCPtr & sequence = field->toSequence();
    // Sequence::operator[] is a plain std::vector index. Nothing inside
    // FieldSet can get here out of range, because the encoder takes its count
    // from getSequenceLength over the same vector, but a consumer supplying
    // its own MessageAccessor whose length disagrees with its entry count is
    // better served by a decode error than by undefined behaviour, and the
    // interface documents no requirement that the two agree.
    if(index >= sequence->size())
    {
      result = false;
    }
    else
    {
      entryAccessor = (*sequence)[index].get();
    }
  }
  return result;
}

void
FieldSet::endSequenceEntry(const FieldIdentity & identity, size_t index, const MessageAccessor * entryAccessor)const
{
}

void
FieldSet::endSequence(const FieldIdentity & identity)const
{
}
