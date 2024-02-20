
#include <coordinate/signed_block.h>
#include <hash.h>

uint256 SignedBlock::GetHash() const
{
    return SerializeHash(*this);
}