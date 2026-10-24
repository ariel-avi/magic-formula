import exampleMf52 from '../../tests/data/mf52_S52.tir?raw';
import examplePac2002 from '../../tests/data/pac2002_S.tir?raw';

export interface Example {
  fileName: string;
  text: string;
}

export const EXAMPLES: readonly Example[] = [
  { fileName: 'pac2002_S.tir', text: examplePac2002 },
  { fileName: 'mf52_S52.tir', text: exampleMf52 },
];
