const plantCards = document.querySelectorAll('.plant-card');
const growthCards = document.querySelectorAll('.growth-card');
const openManualBtn = document.getElementById('openManualBtn');
const manualDialog = document.getElementById('manualDialog');
const openAirQualityBtn = document.getElementById('openAirQualityBtn');
const airQualityDialog = document.getElementById('airQualityDialog');
const airQualitySummary = document.getElementById('airQualitySummary');
const airqTop3 = document.getElementById('airqTop3');
const airqRecommendation = document.getElementById('airqRecommendation');
const doorStatusBadge = document.getElementById('doorStatusBadge');
const doorWhoBadge = document.getElementById('doorWhoBadge');
const introMusicBtn = document.getElementById('introMusicBtn');
const introMusic = document.getElementById('introMusic');
const introCurtain = document.getElementById('introCurtain');
const introConfetti = introCurtain ? introCurtain.querySelector('.intro-confetti') : null;

const GOOGLE_API_KEY = 'AIzaSyDdSwZsbjeH5GSjcil_Oc3XijpFwaJThwM';
const THINGSPEAK_CHANNEL_ID = '3268050';
const THINGSPEAK_READ_API_KEY = '5HXZHUT76XCQGYU0';

const singaporePoints = [
  { name: 'Woodlands', lat: 1.4382, lng: 103.7868 },
  { name: 'Jurong East', lat: 1.3329, lng: 103.7436 },
  { name: 'Tuas', lat: 1.2923, lng: 103.6278 },
  { name: 'Orchard', lat: 1.3048, lng: 103.8318 },
  { name: 'Marina Bay', lat: 1.2834, lng: 103.8607 },
  { name: 'Bedok', lat: 1.3236, lng: 103.9273 },
  { name: 'Tampines', lat: 1.3496, lng: 103.9568 },
  { name: 'Changi', lat: 1.3644, lng: 103.9915 },
];

let mapInstance = null;
let mapMarkers = [];
let mapsLoaded = false;
let mapsLoadPromise = null;

function normalizeField(value) {
  return String(value || '')
    .trim()
    .toUpperCase();
}

function parseDoorStatus(field1) {
  const v = normalizeField(field1);
  if (v === '1' || v === 'OPEN') return 'OPEN';
  return 'CLOSE';
}

function parseWhoField(field2) {
  const v = normalizeField(field2);
  if (v === '2' || v === 'ADMIN') return 'ADMIN';
  if (v === '1' || v === 'USER') return 'USER';
  return 'NONE';
}

function parsePlantField(field3) {
  const v = normalizeField(field3);
  if (v === '1') return 'water_lily';
  if (v === '2') return 'peace_lily';
  if (v === '3') return 'spider_lily';
  if (v === 'PEACE_LILY') return 'peace_lily';
  if (v === 'SPIDER_LILY') return 'spider_lily';
  return 'water_lily';
}

function parseDayField(field4) {
  const day = Number(String(field4 || '').trim());
  if (Number.isInteger(day) && day >= 1 && day <= 5) return day;
  return 1;
}

function setActiveGrowthDay(day) {
  growthCards.forEach((card) => {
    const cardDay = Number(card.dataset.day);
    const isActive = cardDay === day;
    card.classList.toggle('active', isActive);
    if (card.hasAttribute('aria-pressed')) {
      card.setAttribute('aria-pressed', isActive ? 'true' : 'false');
    }
  });
}

function setDoorUi(status, who) {
  if (doorStatusBadge) {
    const close = status !== 'OPEN';
    doorStatusBadge.textContent = close ? 'CLOSE' : 'OPEN';
    doorStatusBadge.classList.toggle('state-open', !close);
    doorStatusBadge.classList.toggle('state-close', close);
  }

  if (doorWhoBadge) {
    const normalizedWho = who === 'ADMIN' ? 'ADMIN' : who === 'USER' ? 'USER' : 'NONE';
    doorWhoBadge.textContent = normalizedWho;
    doorWhoBadge.classList.toggle('role-admin', normalizedWho === 'ADMIN');
    doorWhoBadge.classList.toggle('role-user', normalizedWho !== 'ADMIN');
  }
}

async function refreshThingSpeakState() {
  if (!THINGSPEAK_CHANNEL_ID || THINGSPEAK_CHANNEL_ID.includes('PUT_YOUR')) return;

  const url = `https://api.thingspeak.com/channels/${encodeURIComponent(THINGSPEAK_CHANNEL_ID)}/feeds.json?results=1&api_key=${encodeURIComponent(THINGSPEAK_READ_API_KEY)}`;
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`ThingSpeak read failed: ${response.status}`);
  }

  const payload = await response.json();
  const feed = Array.isArray(payload.feeds) && payload.feeds.length ? payload.feeds[0] : null;
  if (!feed) return;

  const status = parseDoorStatus(feed.field1);
  const who = parseWhoField(feed.field2);
  const plant = parsePlantField(feed.field3);
  const day = parseDayField(feed.field4);

  setDoorUi(status, who);
  const selectedCard = document.querySelector(`.plant-card[data-plant="${plant}"]`);
  if (selectedCard) setActivePlant(selectedCard);
  setActiveGrowthDay(day);
}

function toNumber(value) {
  if (typeof value === 'number' && Number.isFinite(value)) {
    return value;
  }
  if (typeof value === 'string') {
    const parsed = Number(value);
    if (Number.isFinite(parsed)) {
      return parsed;
    }
  }
  return null;
}

function formatPlantName(plantKey) {
  return plantKey.replace(/_/g, ' ');
}

function updateGrowthStrip(plantKey) {
  growthCards.forEach((card) => {
    const day = card.dataset.day;
    card.src = `assets/images/day${day}_${plantKey}.jpg`;
    card.alt = `${formatPlantName(plantKey)} day ${day}`;
  });
}

function setActivePlant(selectedCard) {
  const plantKey = selectedCard.dataset.plant;
  if (!plantKey) {
    return;
  }

  plantCards.forEach((card) => {
    const isActive = card === selectedCard;
    card.classList.toggle('active', isActive);
    card.setAttribute('aria-pressed', isActive ? 'true' : 'false');
  });

  updateGrowthStrip(plantKey);
}

function loadMapsScript(apiKey) {
  if (mapsLoaded) {
    return Promise.resolve();
  }

  if (mapsLoadPromise) {
    return mapsLoadPromise;
  }

  mapsLoadPromise = new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = `https://maps.googleapis.com/maps/api/js?key=${encodeURIComponent(apiKey)}&v=weekly`;
    script.async = true;
    script.defer = true;
    script.onload = () => {
      mapsLoaded = true;
      resolve();
    };
    script.onerror = () => {
      reject(new Error('Failed to load Google Maps script.'));
    };
    document.head.appendChild(script);
  });

  return mapsLoadPromise;
}

async function getPointAirQuality(apiKey, point) {
  const response = await fetch(`https://airquality.googleapis.com/v1/currentConditions:lookup?key=${encodeURIComponent(apiKey)}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      universalAqi: true,
      location: {
        latitude: point.lat,
        longitude: point.lng,
      },
      languageCode: 'en',
    }),
  });

  if (!response.ok) {
    throw new Error(`Air quality request failed: ${response.status}`);
  }

  const data = await response.json();
  const indexes = Array.isArray(data.indexes) ? data.indexes : [];

  let aqi = null;
  let category = 'Unknown';
  let pm25 = null;

  const universal = indexes.find((idx) => idx.code === 'uaqi' || idx.displayName?.toLowerCase().includes('universal'));
  const fallback = indexes[0];
  const picked = universal || fallback;

  if (picked) {
    aqi = toNumber(picked.aqi);
    if (aqi == null) {
      aqi = toNumber(picked.aqiDisplay);
    }
    if (picked.category && picked.category.length) {
      category = picked.category;
    }
  }

  const pollutants = Array.isArray(data.pollutants) ? data.pollutants : [];
  const pm25Pollutant = pollutants.find((p) => {
    const code = String(p.code || '').toLowerCase();
    const display = String(p.displayName || '').toLowerCase();
    return code === 'pm25' || code === 'pm2.5' || display.includes('pm2.5');
  });
  if (pm25Pollutant && pm25Pollutant.concentration) {
    pm25 = toNumber(pm25Pollutant.concentration.value);
  }
  if (pm25 == null && pm25Pollutant) {
    pm25 = toNumber(pm25Pollutant.additionalInfo?.concentration?.value);
  }

  return {
    ...point,
    aqi,
    pm25,
    category,
  };
}

function clearMarkers() {
  mapMarkers.forEach((marker) => marker.setMap(null));
  mapMarkers = [];
}

function markerColor(rank) {
  if (rank === 0) return '#ff3b3b';
  if (rank <= 2) return '#ff8e1a';
  return '#ffd43b';
}

function renderMap(results) {
  const mapNode = document.getElementById('airqMap');
  if (!mapNode || !window.google || !window.google.maps) {
    return;
  }

  if (!mapInstance) {
    mapInstance = new window.google.maps.Map(mapNode, {
      center: { lat: 1.3521, lng: 103.8198 },
      zoom: 11,
      mapTypeControl: false,
      streetViewControl: false,
      fullscreenControl: false,
    });
  }

  clearMarkers();

  const worstThree = results.slice(0, 3);

  worstThree.forEach((item, rank) => {
    const metricParts = [];
    if (item.pm25 != null) {
      metricParts.push(`PM2.5 ${item.pm25.toFixed(1)} µg/m³`);
    }
    if (item.aqi != null) {
      metricParts.push(`AQI ${item.aqi}`);
    }
    const metricText = metricParts.join(' | ');

    const marker = new window.google.maps.Marker({
      position: { lat: item.lat, lng: item.lng },
      map: mapInstance,
      title: `${item.name}: ${metricText}`,
      icon: {
        path: window.google.maps.SymbolPath.CIRCLE,
        scale: rank === 0 ? 9 : 7,
        fillColor: markerColor(rank),
        fillOpacity: 0.96,
        strokeColor: '#141414',
        strokeWeight: 1,
      },
    });

    const infoWindow = new window.google.maps.InfoWindow({
      content: `<strong>${item.name}</strong><br/>${metricParts.join('<br/>')}`,
    });

    marker.addListener('click', () => infoWindow.open({ anchor: marker, map: mapInstance }));
    mapMarkers.push(marker);
  });
}

function renderTopThree(results) {
  if (!airqTop3 || !airqRecommendation) {
    return;
  }

  const valid = results.filter((item) => item.pm25 != null || item.aqi != null);
  const topThree = valid.slice(0, 3);

  airqTop3.innerHTML = '';
  if (topThree.length === 0) {
    const row = document.createElement('li');
    row.textContent = 'No live data available right now.';
    airqTop3.appendChild(row);
    airqRecommendation.textContent = '';
    return;
  }

  topThree.forEach((item) => {
    const row = document.createElement('li');
    const parts = [];
    if (item.pm25 != null) {
      parts.push(`PM2.5 ${item.pm25.toFixed(1)} µg/m³`);
    }
    if (item.aqi != null) {
      parts.push(`AQI ${item.aqi}`);
    }
    row.textContent = `${item.name} - ${parts.join(' | ')}`;
    airqTop3.appendChild(row);
  });

  const names = topThree.map((item) => item.name).join(', ');
  airqRecommendation.textContent = `Worst air quality places in Singapore right now are ${names}. Help improve Singapore's air quality by placing plants in these three communities.`;
}

async function runAirQualityScan() {
  if (airqTop3) {
    airqTop3.innerHTML = '<li>Loading air quality data...</li>';
  }
  if (airqRecommendation) {
    airqRecommendation.textContent = '';
  }

  await loadMapsScript(GOOGLE_API_KEY);

  const rawResults = await Promise.all(
    singaporePoints.map((point) =>
      getPointAirQuality(GOOGLE_API_KEY, point).catch(() => ({ ...point, aqi: null, category: 'Unavailable' }))
    )
  );

  const sorted = [...rawResults].sort((a, b) => {
    const aValue = a.pm25 == null ? (a.aqi == null ? -1 : a.aqi) : a.pm25;
    const bValue = b.pm25 == null ? (b.aqi == null ? -1 : b.aqi) : b.pm25;
    return bValue - aValue;
  });

  const displayable = sorted.filter((item) => item.pm25 != null || item.aqi != null);
  renderMap(displayable);
  renderTopThree(displayable);

  if (airQualitySummary) {
    const worst = displayable[0];
    if (worst && worst.aqi != null) {
      airQualitySummary.textContent = `Worst spot: ${worst.name} (AQI ${worst.aqi})`;
    } else {
      airQualitySummary.textContent = 'Worst spot: unavailable';
    }
  }
}

plantCards.forEach((card) => {
  card.addEventListener('click', () => setActivePlant(card));
  card.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      setActivePlant(card);
    }
  });
});

growthCards.forEach((card) => {
  const day = Number(card.dataset.day);
  if (!Number.isFinite(day)) return;

  card.addEventListener('click', () => setActiveGrowthDay(day));
  card.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      setActiveGrowthDay(day);
    }
  });
});

const defaultCard = document.querySelector('.plant-card.active') || plantCards[0];
if (defaultCard) {
  setActivePlant(defaultCard);
}

refreshThingSpeakState().catch(() => {});
setInterval(() => {
  refreshThingSpeakState().catch(() => {});
}, 20000);

if (openManualBtn && manualDialog && typeof manualDialog.showModal === 'function') {
  openManualBtn.addEventListener('click', () => {
    manualDialog.showModal();
  });

  manualDialog.addEventListener('click', (event) => {
    const bounds = manualDialog.getBoundingClientRect();
    const isOutside =
      event.clientX < bounds.left ||
      event.clientX > bounds.right ||
      event.clientY < bounds.top ||
      event.clientY > bounds.bottom;

    if (isOutside) {
      manualDialog.close();
    }
  });
}

if (openAirQualityBtn && airQualityDialog && typeof airQualityDialog.showModal === 'function') {
  openAirQualityBtn.addEventListener('click', async () => {
    airQualityDialog.showModal();
    try {
      await runAirQualityScan();
    } catch (error) {
      if (airqTop3) {
        airqTop3.innerHTML = '<li>Could not load air quality right now. Check API key/billing and try again.</li>';
      }
      if (airQualitySummary) {
        airQualitySummary.textContent = 'API error';
      }
      if (airqRecommendation) {
        airqRecommendation.textContent = '';
      }
    }
  });

  airQualityDialog.addEventListener('click', (event) => {
    const bounds = airQualityDialog.getBoundingClientRect();
    const isOutside =
      event.clientX < bounds.left ||
      event.clientX > bounds.right ||
      event.clientY < bounds.top ||
      event.clientY > bounds.bottom;

    if (isOutside) {
      airQualityDialog.close();
    }
  });
}

if (introMusicBtn && introMusic && introCurtain) {
  const clearIntro = () => {
    introCurtain.classList.remove('active');
    if (introConfetti) introConfetti.classList.add('hidden');
  };

  introMusicBtn.addEventListener('click', async () => {
    if (!introMusic.paused) return;

    introCurtain.classList.add('active');
    if (introConfetti) introConfetti.classList.remove('hidden');

    try {
      introMusic.currentTime = 0;
      await introMusic.play();
    } catch (error) {
      clearIntro();
    }
  });

  introMusic.addEventListener('ended', () => {
    clearIntro();
  });

  introMusic.addEventListener('pause', () => {
    if (introMusic.currentTime < introMusic.duration) {
      clearIntro();
    }
  });
}
