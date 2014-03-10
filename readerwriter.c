//  return 1 if obtained,
//      0 otherwise

int bt_spinwritetry(BtSpinLatch *latch)
{
ushort prev;

#ifdef  unix
    prev = __sync_fetch_and_or((ushort *)latch, XCL);
#else
    _InterlockedOr16((ushort *)latch, XCL);
#endif
    //  take write access if all bits are clear

    if( !(prev & XCL) )
      if( !(prev & ~BOTH) )
        return 1;
      else
#ifdef unix
        __sync_fetch_and_and ((ushort *)latch, ~XCL);
#else
        _InterlockedAnd16((ushort *)latch, ~XCL);
#endif
    return 0;
}

//  clear write mode

void bt_spinreleasewrite(BtSpinLatch *latch)
{
#ifdef unix
    __sync_fetch_and_and((ushort *)latch, ~BOTH);
#else
    _InterlockedAnd16((ushort *)latch, ~BOTH);
#endif
}

//  decrement reader count

void bt_spinreleaseread(BtSpinLatch *latch)
{
