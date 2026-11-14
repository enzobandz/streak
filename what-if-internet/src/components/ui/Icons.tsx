import React from 'react';

// Original line-icon set (24×24 grid, 2px strokes) used throughout the episode.
const P: Record<string, React.ReactNode> = {
  phone: (<><rect x="7" y="2.5" width="10" height="19" rx="2.2" /><path d="M11 18.5h2" /></>),
  wifiOff: (<><path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0" /><circle cx="12" cy="19" r="1" /><path d="M3 3l18 18" /></>),
  wifi: (<><path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0" /><circle cx="12" cy="19" r="1" /></>),
  card: (<><rect x="2.5" y="5.5" width="19" height="13" rx="2" /><path d="M2.5 9.5h19M6 15h4" /></>),
  cash: (<><rect x="2.5" y="6" width="19" height="12" rx="1.5" /><circle cx="12" cy="12" r="2.6" /><path d="M6 9v.01M18 15v.01" /></>),
  bank: (<><path d="M3 9.5L12 4l9 5.5M4.5 10v8M9 10v8M15 10v8M19.5 10v8M3 20h18" /></>),
  plane: (<><path d="M21 15.5l-8-4.5V5a1 1 0 0 0-2 0v6l-8 4.5V17l8-2.5V19l-2.5 1.5V22L12 21l3.5 1v-1.5L13 19v-4.5l8 2.5z" /></>),
  ship: (<><path d="M3 15l2 5h14l2-5H3zM6 15V10h12v5M9 10V6h6v4" /></>),
  truck: (<><path d="M2.5 6h11v10h-11zM13.5 9.5h4l3 3.5V16h-7z" /><circle cx="6.5" cy="17.5" r="1.8" /><circle cx="17" cy="17.5" r="1.8" /></>),
  hospital: (<><rect x="3.5" y="3.5" width="17" height="17" rx="2" /><path d="M12 8v8M8 12h8" /></>),
  bolt: (<><path d="M13 2.5L5 13.5h6l-1 8 8-11h-6z" /></>),
  water: (<><path d="M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z" /></>),
  radio: (<><rect x="3" y="8.5" width="18" height="12" rx="2" /><circle cx="15.5" cy="14.5" r="3" /><path d="M6.5 12.5h4M6.5 16.5h4M7 8.5L17 3.5" /></>),
  tv: (<><rect x="2.5" y="5.5" width="19" height="12.5" rx="2" /><path d="M8 21h8M9 2.5l3 3 3-3" /></>),
  mail: (<><rect x="2.5" y="5" width="19" height="14" rx="2" /><path d="M3 6.5l9 6.5 9-6.5" /></>),
  doc: (<><path d="M6 2.5h8l4.5 4.5V21.5H6z" /><path d="M14 2.5V7h4.5M9 12h6M9 16h6" /></>),
  video: (<><rect x="2.5" y="6" width="13" height="12" rx="2" /><path d="M15.5 10.5L21.5 7v10l-6-3.5z" /></>),
  payroll: (<><rect x="3" y="3" width="18" height="18" rx="2" /><path d="M12 7v10M14.5 9c-.5-1-1.5-1.3-2.5-1.3-1.5 0-2.5.8-2.5 1.9 0 2.8 5 1.4 5 4.2 0 1.1-1 2-2.5 2-1.1 0-2.2-.4-2.7-1.4" /></>),
  cart: (<><path d="M2.5 3.5h3l2.4 11.5h10.6l2-8H7" /><circle cx="9.5" cy="19.5" r="1.5" /><circle cx="17" cy="19.5" r="1.5" /></>),
  check: (<><path d="M4.5 12.5l5 5 10-11" /></>),
  x: (<><path d="M5.5 5.5l13 13M18.5 5.5l-13 13" /></>),
  clock: (<><circle cx="12" cy="12" r="9.5" /><path d="M12 6.5V12l3.5 2.5" /></>),
  globe: (<><circle cx="12" cy="12" r="9.5" /><path d="M2.5 12h19M12 2.5c3 3.2 3 15.8 0 19M12 2.5c-3 3.2-3 15.8 0 19" /></>),
  router: (<><rect x="2.5" y="13" width="19" height="7" rx="2" /><path d="M6.5 16.5h.01M10 16.5h.01M8 13l-2-6M16 13l2-6" /></>),
  pin: (<><path d="M12 21.5s7-6.2 7-12a7 7 0 0 0-14 0c0 5.8 7 12 7 12z" /><circle cx="12" cy="9.5" r="2.5" /></>),
  satellite: (<><path d="M9 9l6 6M7.5 10.5l-5-5 3-3 5 5M13.5 16.5l5 5 3-3-5-5" /><rect x="8.5" y="8.5" width="7" height="7" rx="1" transform="rotate(45 12 12)" /></>),
  users: (<><circle cx="9" cy="8" r="3.5" /><path d="M2.5 20c.5-3.5 3.2-5.5 6.5-5.5s6 2 6.5 5.5" /><circle cx="17" cy="9" r="2.6" /><path d="M16.5 14.6c2.6.2 4.6 2 5 4.9" /></>),
  notebook: (<><rect x="5" y="2.5" width="14" height="19" rx="1.5" /><path d="M5 7H3M5 12H3M5 17H3M9 7.5h6M9 11.5h6" /></>),
  chart: (<><path d="M3 20.5h18M6 17V11M10.5 17V6.5M15 17v-4M19.5 17V8.5" /></>),
  server: (<><rect x="3.5" y="3" width="17" height="7.5" rx="1.5" /><rect x="3.5" y="13.5" width="17" height="7.5" rx="1.5" /><path d="M7 6.8h.01M7 17.3h.01M11 6.8h6M11 17.3h6" /></>),
  cable: (<><path d="M2 14c4-6 8 6 12 0s6-4 8-2" /><circle cx="12" cy="13" r="8" opacity="0" /></>),
  network: (<><circle cx="12" cy="5" r="2.3" /><circle cx="5" cy="18" r="2.3" /><circle cx="19" cy="18" r="2.3" /><path d="M11 7l-5 9M13 7l5 9M7.3 18h9.4" /></>),
  building: (<><path d="M4 21.5V5.5l8-3v19M12 8.5l8 3v10M2.5 21.5h19M7 9h2M7 13h2M7 17h2M15 14h2M15 17.5h2" /></>),
  school: (<><path d="M2 9l10-5 10 5-10 5zM6 11v5c3 2.5 9 2.5 12 0v-5" /></>),
  cloud: (<><path d="M7 18.5a4.5 4.5 0 0 1-.6-8.96A6 6 0 0 1 18 9a4.5 4.5 0 0 1-.5 9.5z" /></>),
  gov: (<><path d="M3 21h18M4.5 18h15M6 18v-7M10 18v-7M14 18v-7M18 18v-7M3 9l9-6 9 6z" /></>),
  factory: (<><path d="M2.5 21V10l6 3.5V10l6 3.5V6l3-2v17zM2.5 21h19" /></>),
  family: (<><circle cx="7" cy="6" r="2.5" /><circle cx="17" cy="6" r="2.5" /><circle cx="12" cy="11.5" r="2" /><path d="M3 20c0-4 2-7 4-7s4 3 4 7M13 20c0-4 2-7 4-7s4 3 4 7M10 20c0-2.5.8-4.5 2-4.5s2 2 2 4.5" /></>),
  alert: (<><path d="M12 3L2 20.5h20z" /><path d="M12 9.5v5M12 17.5v.01" /></>),
  spinner: (<><path d="M12 3a9 9 0 1 1-9 9" /></>),
  map: (<><path d="M3 6l6-2.5 6 2.5 6-2.5v14.5l-6 2.5-6-2.5-6 2.5z" /><path d="M9 3.5V18M15 6v14.5" /></>),
  shield: (<><path d="M12 2.5l8 3v6c0 5-3.4 8.6-8 10-4.6-1.4-8-5-8-10v-6z" /><path d="M8.5 12l2.5 2.5 4.5-5" /></>),
  comment: (<><path d="M4 4.5h16v11H9.5L5 19.5v-4H4z" /></>),
  bell: (<><path d="M6 16.5V11a6 6 0 0 1 12 0v5.5l1.5 2h-15zM10 20.5a2 2 0 0 0 4 0" /></>),
  power: (<><path d="M12 2.5v9M6.4 5.8a8 8 0 1 0 11.2 0" /></>),
  hourglass: (<><path d="M6 2.5h12M6 21.5h12M7 2.5c0 5 10 6 10 9.5S7 16.5 7 21.5M17 2.5c0 5-10 6-10 9.5s10 4.5 10 9.5" /></>),
};

export type IconName = keyof typeof P;

export const Icon: React.FC<{name: IconName | string; size?: number; color?: string; stroke?: number; style?: React.CSSProperties}> = ({
  name,
  size = 48,
  color = 'currentColor',
  stroke = 2,
  style,
}) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" stroke={color} strokeWidth={stroke} strokeLinecap="round" strokeLinejoin="round" style={style}>
    {P[name as IconName] ?? P.alert}
  </svg>
);
