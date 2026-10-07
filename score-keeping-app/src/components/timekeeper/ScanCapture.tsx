'use client';

import { useEffect } from 'react';
import { createScanCapture } from '@/lib/scanner/scanCapture';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectIsScannerListening } from '@/store/timekeeperSelectors';
import { handleScannedCard } from '@/store/timekeeperThunks';

/** Detached while a shooter is armed or shooting, so a stray scan cannot replace the active turn. */
export const ScanCapture = () => {
  const dispatch = useAppDispatch();
  const isListening = useAppSelector(selectIsScannerListening);

  useEffect(() => {
    if (!isListening) {
      return;
    }

    return createScanCapture(scan => dispatch(handleScannedCard(scan.code)));
  }, [isListening, dispatch]);

  return null;
};
