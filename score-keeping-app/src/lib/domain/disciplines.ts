export const DISCIPLINES = [
  'OKP',
  'OKKP',
  'SKP',
  'SKKP',
  'PCC 9mm',
  'PCC .22',
  'OKR',
  'OKKR',
  'SKR',
  'SKKR'
] as const;

export type Discipline = (typeof DISCIPLINES)[number];
