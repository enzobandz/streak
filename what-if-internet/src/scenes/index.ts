import React from 'react';
import {ColdOpen} from './ColdOpen';
import {Title} from './Title';
import {Network} from './Network';
import {FirstMinutes} from './FirstMinutes';

export const SCENE_COMPONENTS: Record<string, React.FC> = {
  coldOpen: ColdOpen,
  title: Title,
  network: Network,
  firstMinutes: FirstMinutes,
};
