import React from 'react';
import {Composition} from 'remotion';
import {Episode} from './Episode';
import {TIMELINE} from './lib/timeline';
import {SCENE_COMPONENTS} from './scenes';

const {fps, width, height} = TIMELINE;

export const RemotionRoot: React.FC = () => {
  const poc = TIMELINE.scenes.filter((s) => s.id === 'coldOpen' || s.id === 'title');
  const pocDur = poc.reduce((a, s) => a + s.duration, 0);
  const all = TIMELINE.scenes.every((s) => SCENE_COMPONENTS[s.id]);
  return (
    <>
      {all ? <Composition id="Episode" component={Episode} durationInFrames={TIMELINE.durationInFrames} fps={fps} width={width} height={height} /> : null}
      <Composition
        id="ProofOfConcept"
        component={Episode}
        durationInFrames={pocDur}
        fps={fps}
        width={width}
        height={height}
        defaultProps={{sceneIds: ['coldOpen', 'title'], fadeOut: true}}
      />
      {/* One composition per scene for fast iteration in Remotion Studio */}
      {TIMELINE.scenes
        .filter((s) => SCENE_COMPONENTS[s.id])
        .map((s) => (
          <Composition key={s.id} id={`Scene-${s.id}`} component={Episode} durationInFrames={s.duration} fps={fps} width={width} height={height} defaultProps={{sceneIds: [s.id]}} />
        ))}
    </>
  );
};
