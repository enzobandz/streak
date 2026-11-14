import React from 'react';
import {ColdOpen} from './ColdOpen';
import {Title} from './Title';
import {Network} from './Network';
import {FirstMinutes} from './FirstMinutes';
import {Money} from './Money';
import {Transport} from './Transport';
import {Critical} from './Critical';
import {Work} from './Work';
import {Human} from './Human';
import {Restart} from './Restart';
import {Outro} from './Outro';

/** Scene id (from src/data/script.json) -> component. */
export const SCENE_COMPONENTS: Record<string, React.FC> = {
  coldOpen: ColdOpen,
  title: Title,
  network: Network,
  firstMinutes: FirstMinutes,
  money: Money,
  transport: Transport,
  critical: Critical,
  work: Work,
  human: Human,
  restart: Restart,
  outro: Outro,
};
