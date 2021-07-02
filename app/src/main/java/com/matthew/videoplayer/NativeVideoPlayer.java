package com.matthew.videoplayer;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.storage.OnObbStateChangeListener;
import android.os.storage.StorageManager;
import android.util.Log;
import android.view.Surface;

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.C.ContentType;
import com.google.android.exoplayer2.DefaultRenderersFactory;
import com.google.android.exoplayer2.ExoPlayerFactory;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.PlaybackParameters;
import com.google.android.exoplayer2.Player;
import com.google.android.exoplayer2.Renderer;
import com.google.android.exoplayer2.SimpleExoPlayer;
import com.google.android.exoplayer2.Timeline;
import com.google.android.exoplayer2.audio.AudioRendererEventListener;
import com.google.android.exoplayer2.audio.AudioSink;
import com.google.android.exoplayer2.drm.DefaultDrmSessionManager;
import com.google.android.exoplayer2.drm.DrmSessionManager;
import com.google.android.exoplayer2.drm.FrameworkMediaCrypto;
import com.google.android.exoplayer2.drm.FrameworkMediaDrm;
import com.google.android.exoplayer2.drm.HttpMediaDrmCallback;
import com.google.android.exoplayer2.drm.UnsupportedDrmException;
import com.google.android.exoplayer2.metadata.MetadataOutput;
import com.google.android.exoplayer2.source.ExtractorMediaSource;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.dash.DefaultDashChunkSource;
import com.google.android.exoplayer2.source.dash.DashMediaSource;
import com.google.android.exoplayer2.source.hls.HlsMediaSource;
import com.google.android.exoplayer2.source.smoothstreaming.DefaultSsChunkSource;
import com.google.android.exoplayer2.source.smoothstreaming.SsMediaSource;
import com.google.android.exoplayer2.text.TextOutput;
import com.google.android.exoplayer2.trackselection.AdaptiveTrackSelection;
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import com.google.android.exoplayer2.trackselection.TrackSelection;
import com.google.android.exoplayer2.upstream.BandwidthMeter;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.android.exoplayer2.upstream.DefaultBandwidthMeter;
import com.google.android.exoplayer2.upstream.DefaultDataSourceFactory;
import com.google.android.exoplayer2.upstream.DefaultHttpDataSourceFactory;
import com.google.android.exoplayer2.upstream.FileDataSourceFactory;
import com.google.android.exoplayer2.upstream.HttpDataSource;
import com.google.android.exoplayer2.upstream.cache.Cache;
import com.google.android.exoplayer2.upstream.cache.CacheDataSource;
import com.google.android.exoplayer2.upstream.cache.CacheDataSourceFactory;
import com.google.android.exoplayer2.upstream.cache.NoOpCacheEvictor;
import com.google.android.exoplayer2.upstream.cache.SimpleCache;
import com.google.android.exoplayer2.util.Util;
import com.google.android.exoplayer2.video.VideoRendererEventListener;
import com.twobigears.audio360.AudioEngine;
import com.twobigears.audio360.ChannelMap;
import com.twobigears.audio360.SpatDecoderQueue;
import com.twobigears.audio360.TBQuat;
import com.twobigears.audio360exo2.Audio360Sink;
import com.twobigears.audio360exo2.OpusRenderer;

import java.io.File;
import java.lang.Math;
import java.lang.System;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Hashtable;
import java.util.UUID;

import com.unity3d.player.UnityPlayer;

public class NativeVideoPlayer {
    private static final String TAG = "NativeVideoPlayer";

    static final float SAMPLE_RATE = 48000.f;
    static final int BUFFER_SIZE = 1024;
    static final int QUEUE_SIZE_IN_SAMPLES = 40960;

    static Handler handler;
    static File downloadDirectory;
    static Cache downloadCache;

    private static class VideoPlayer {
        private SimpleExoPlayer exoPlayer;
        private AudioEngine engine;
        private SpatDecoderQueue spat;
        private AudioSink audio360Sink;
        private FrameworkMediaDrm mediaDrm;
        private Surface mSurface;
        private String filePath;
        private boolean readyToPlay;
        private volatile boolean isPlaying;
        private volatile int currentPlaybackState;
        private volatile int stereoMode = -1;
        private volatile int width;
        private volatile int height;
        private volatile long duration;
        private volatile long lastPlaybackPosition;
        private volatile long lastPlaybackUpdateTime;
        private volatile float lastPlaybackSpeed;
    }

    static Hashtable<String, VideoPlayer> videoPlayers = new Hashtable<String, VideoPlayer>();

    private static void updatePlaybackState(VideoPlayer currPlayer) {
        currPlayer.duration = currPlayer.exoPlayer.getDuration();
        currPlayer.lastPlaybackPosition = currPlayer.exoPlayer.getCurrentPosition();
        currPlayer.lastPlaybackSpeed = currPlayer.isPlaying ? currPlayer.exoPlayer.getPlaybackParameters().speed : 0;
        currPlayer.lastPlaybackUpdateTime = System.currentTimeMillis();
        Format format = currPlayer.exoPlayer.getVideoFormat();
        if (format != null) {
            currPlayer.stereoMode = format.stereoMode;
            currPlayer.width = format.width;
            currPlayer.height = format.height;
        } else {
            currPlayer.stereoMode = -1;
            currPlayer.width = 0;
            currPlayer.height = 0;
        }
    }

    private static Handler getHandler() {
        if (handler == null) {
            handler = new Handler(Looper.getMainLooper());
        }

        return handler;
    }

    private static class CustomRenderersFactory extends DefaultRenderersFactory {
        private Context myContext;
        private long myAllowedVideoJoiningTimeMs = 5000;

        public CustomRenderersFactory(Context context) {
            super(context);
            this.myContext = context;
        }

        //@Override
        public DefaultRenderersFactory setAllowedVideoJoiningTimeMs(long allowedVideoJoiningTimeMs) {
            //super.setAllowedVideoJoiningTimeMs(allowedVideoJoiningTimeMs);
            myAllowedVideoJoiningTimeMs = allowedVideoJoiningTimeMs;
            return this;
        }

        public Renderer[] createRenderers(
                Handler eventHandler,
                VideoRendererEventListener videoRendererEventListener,
                AudioRendererEventListener audioRendererEventListener,
                TextOutput textRendererOutput,
                MetadataOutput metadataRendererOutput,
                /*@Nullable*/ DrmSessionManager<FrameworkMediaCrypto> drmSessionManager,
                VideoPlayer currPlayer) {

            Renderer[] renderers = super.createRenderers(
                    eventHandler,
                    videoRendererEventListener,
                    audioRendererEventListener,
                    textRendererOutput,
                    metadataRendererOutput,
                    drmSessionManager);

            ArrayList<Renderer> rendererList = new ArrayList<Renderer>(Arrays.asList(renderers));

            // The output latency of the engine can be used to compensate for sync
            double latency = currPlayer.engine.getOutputLatencyMs();

            // Audio: opus codec with the spatial audio engine
            // TBE_8_2 implies 10 channels of audio (8 channels of spatial audio, 2 channels of head-locked)
            currPlayer.audio360Sink = new Audio360Sink(currPlayer.spat, ChannelMap.TBE_8_2, latency);
            final OpusRenderer audioRenderer = new OpusRenderer(currPlayer.audio360Sink);

            rendererList.add(audioRenderer);

            renderers = rendererList.toArray(renderers);
            return renderers;
        }
    }

    private static File getDownloadDirectory(Context context) {
        if (downloadDirectory == null) {
            downloadDirectory = context.getExternalFilesDir(null);
            if (downloadDirectory == null) {
                downloadDirectory = context.getFilesDir();
            }
        }
        return downloadDirectory;
    }

    private static synchronized Cache getDownloadCache(Context context) {
        if (downloadCache == null) {
            File downloadContentDirectory = new File(getDownloadDirectory(context), "downloads");
            downloadCache = new SimpleCache(downloadContentDirectory, new NoOpCacheEvictor());
        }
        return downloadCache;
    }

    private static CacheDataSourceFactory buildReadOnlyCacheDataSource(
            DefaultDataSourceFactory upstreamFactory, Cache cache) {
        return new CacheDataSourceFactory(
                cache,
                upstreamFactory,
                new FileDataSourceFactory(),
                /* cacheWriteDataSinkFactory= */ null,
                CacheDataSource.FLAG_IGNORE_CACHE_ON_ERROR,
                /* eventListener= */ null);
    }

    /**
     * Returns a {@link DataSource.Factory}.
     */
    public static DataSource.Factory buildDataSourceFactory(Context context, VideoPlayer currPlayer) {
        DefaultDataSourceFactory upstreamFactory =
                new DefaultDataSourceFactory(context, null, buildHttpDataSourceFactory(context));
        return buildReadOnlyCacheDataSource(upstreamFactory, getDownloadCache(context));
    }

    /**
     * Returns a {@link HttpDataSource.Factory}.
     */
    public static HttpDataSource.Factory buildHttpDataSourceFactory(Context context) {
        return new DefaultHttpDataSourceFactory(Util.getUserAgent(context, "NativeVideoPlayer"));
    }

    private static DefaultDrmSessionManager<FrameworkMediaCrypto> buildDrmSessionManagerV18(Context context,
                                                                                            UUID uuid, String licenseUrl,
                                                                                            String[] keyRequestPropertiesArray,
                                                                                            boolean multiSession,
                                                                                            VideoPlayer currplayer)
            throws UnsupportedDrmException {
        HttpDataSource.Factory licenseDataSourceFactory = buildHttpDataSourceFactory(context);
        HttpMediaDrmCallback drmCallback =
                new HttpMediaDrmCallback(licenseUrl, licenseDataSourceFactory);
        if (keyRequestPropertiesArray != null) {
            for (int i = 0; i < keyRequestPropertiesArray.length - 1; i += 2) {
                drmCallback.setKeyRequestProperty(keyRequestPropertiesArray[i],
                        keyRequestPropertiesArray[i + 1]);
            }
        }
        if (currplayer.mediaDrm != null) {
            currplayer.mediaDrm.release();
        }
        currplayer.mediaDrm = FrameworkMediaDrm.newInstance(uuid);
        return new DefaultDrmSessionManager<>(uuid, currplayer.mediaDrm, drmCallback, null, multiSession);
    }

    @SuppressWarnings("unchecked")
    private static MediaSource buildMediaSource(Context context, Uri uri, /*@Nullable*/ String overrideExtension, DataSource.Factory dataSourceFactory) {
        @ContentType int type = Util.inferContentType(uri, overrideExtension);
        switch (type) {
            case C.TYPE_DASH:

                return new DashMediaSource.Factory(new DefaultDashChunkSource.Factory(dataSourceFactory), dataSourceFactory)
                        .createMediaSource(uri);
            case C.TYPE_SS:
                return new SsMediaSource.Factory(new DefaultSsChunkSource.Factory(dataSourceFactory), dataSourceFactory)
                        .createMediaSource(uri);
            case C.TYPE_HLS:
                return new HlsMediaSource.Factory(dataSourceFactory)
                        .createMediaSource(uri);
            case C.TYPE_OTHER:
                return new ExtractorMediaSource.Factory(dataSourceFactory).createMediaSource(uri);
            default: {
                throw new IllegalStateException("Unsupported type: " + type);
            }
        }
    }

    public static void Log(String message) {
        Log.d(TAG, message);
    }

    public static void Prepare(String videoID, String filePath){
        if (!videoPlayers.containsKey(videoID)) {
            VideoPlayer videoPlayer = new VideoPlayer();
            videoPlayer.filePath = filePath;
            videoPlayers.put(videoID, videoPlayer);
            Log.d(TAG, "Added video player: " + videoID);
        }
    }

    public static void CreateSurface(Surface surface, String videoID, String textureID) {

        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        //send videoID and textureID back to unity to create external texture
        UnityPlayer.UnitySendMessage("ExoPlayerUnity", "CreateOESTexture", videoID + "," + textureID);

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        currVideoPlayer.mSurface = surface;

        // set up exoplayer on main thread
        getHandler().post(new Runnable() {
            @Override
            public void run() {
                // 1. Create a default TrackSelector
                BandwidthMeter bandwidthMeter = new DefaultBandwidthMeter();
                TrackSelection.Factory videoTrackSelectionFactory =
                        new AdaptiveTrackSelection.Factory(bandwidthMeter);
                DefaultTrackSelector trackSelector =
                        new DefaultTrackSelector(videoTrackSelectionFactory);
                // Produces DataSource instances through which media data is loaded.
                DataSource.Factory dataSourceFactory = buildDataSourceFactory(UnityPlayer.currentActivity, currVideoPlayer);

                Uri uri = Uri.parse(currVideoPlayer.filePath);

                if (currVideoPlayer.filePath.startsWith("jar:file:")) {
                    if (currVideoPlayer.filePath.contains(".apk")) { // APK
                        uri = new Uri.Builder().scheme("asset").path(currVideoPlayer.filePath.substring(currVideoPlayer.filePath.indexOf("/assets/") + "/assets/".length())).build();
                    } else if (currVideoPlayer.filePath.contains(".obb")) { // OBB
                        String obbPath = currVideoPlayer.filePath.substring(11, currVideoPlayer.filePath.indexOf(".obb") + 4);

                        StorageManager sm = (StorageManager) UnityPlayer.currentActivity.getSystemService(Context.STORAGE_SERVICE);
                        if (!sm.isObbMounted(obbPath)) {
                            sm.mountObb(obbPath, null, new OnObbStateChangeListener() {
                                @Override
                                public void onObbStateChange(String path, int state) {
                                    super.onObbStateChange(path, state);
                                }
                            });
                        }

                        uri = new Uri.Builder().scheme("file").path(sm.getMountedObbPath(obbPath) + currVideoPlayer.filePath.substring(currVideoPlayer.filePath.indexOf(".obb") + 5)).build();
                    }
                }

                // Set up video source if drmLicenseUrl is set
                DefaultDrmSessionManager<FrameworkMediaCrypto> drmSessionManager = null;

                // This is the MediaSource representing the media to be played.
                MediaSource videoSource = buildMediaSource(UnityPlayer.currentActivity, uri, null, dataSourceFactory);

                Log.d(TAG, "Requested play of " + currVideoPlayer.filePath + " uri: " + uri.toString());

                if (currVideoPlayer.engine == null) {
                    currVideoPlayer.engine = AudioEngine.create(SAMPLE_RATE, BUFFER_SIZE, QUEUE_SIZE_IN_SAMPLES, UnityPlayer.currentActivity);
                    currVideoPlayer.spat = currVideoPlayer.engine.createSpatDecoderQueue();
                    currVideoPlayer.engine.start();
                }

                // Create our modified ExoPlayer instance
                if (currVideoPlayer.exoPlayer != null) {
                    currVideoPlayer.exoPlayer.release();
                }
                currVideoPlayer.exoPlayer = ExoPlayerFactory.newSimpleInstance(UnityPlayer.currentActivity, new CustomRenderersFactory(UnityPlayer.currentActivity), trackSelector, drmSessionManager);

                currVideoPlayer.exoPlayer.addListener(new Player.DefaultEventListener() {

                    @Override
                    public void onPlayerStateChanged(boolean playWhenReady, int playbackState) {

                        //call on prepared from unity
                        if (!currVideoPlayer.readyToPlay && playbackState == Player.STATE_READY){
                            currVideoPlayer.readyToPlay = true;
                            UnityPlayer.UnitySendMessage("ExoPlayerUnity", "OnVideoPrepared", videoID);
                        }

                        currVideoPlayer.isPlaying = playWhenReady && (playbackState == Player.STATE_READY || playbackState == Player.STATE_BUFFERING);
                        currVideoPlayer.currentPlaybackState = playbackState;
                        updatePlaybackState(currVideoPlayer);
                    }

                    @Override
                    public void onPlaybackParametersChanged(PlaybackParameters params) {
                        updatePlaybackState(currVideoPlayer);
                    }

                    @Override
                    public void onPositionDiscontinuity(int reason) {
                        updatePlaybackState(currVideoPlayer);
                    }
                });

                currVideoPlayer.exoPlayer.setVideoSurface(currVideoPlayer.mSurface);

                // Prepare the player with the source.
                currVideoPlayer.exoPlayer.prepare(videoSource);

                //dev hack
                currVideoPlayer.exoPlayer.setRepeatMode(Player.REPEAT_MODE_ONE);
                currVideoPlayer.exoPlayer.setPlayWhenReady(false);
            }
        });
    }

    public static void SetLooping(final boolean looping, final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        getHandler().post(new Runnable() {
            @Override
            public void run() {
                if (currVideoPlayer.exoPlayer != null) {
                    if (looping) {
                        currVideoPlayer.exoPlayer.setRepeatMode(Player.REPEAT_MODE_ONE);
                    } else {
                        currVideoPlayer.exoPlayer.setRepeatMode(Player.REPEAT_MODE_OFF);
                    }
                }
            }
        });
    }

    public static void Stop(final String videoID) {

        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        getHandler().post(new Runnable() {
            @Override
            public void run() {
                if (currVideoPlayer.exoPlayer != null) {
                    currVideoPlayer.exoPlayer.stop();
                    currVideoPlayer.exoPlayer.release();
                    currVideoPlayer.exoPlayer = null;
                }
                if (currVideoPlayer.mediaDrm != null) {
                    currVideoPlayer.mediaDrm.release();
                    currVideoPlayer.mediaDrm = null;
                }
                if (currVideoPlayer.engine != null) {
                    currVideoPlayer.engine.destroySpatDecoderQueue(currVideoPlayer.spat);
                    currVideoPlayer.engine.delete();
                    currVideoPlayer.spat = null;
                    currVideoPlayer.engine = null;
                }

                videoPlayers.remove(videoID);
            }
        });
    }

    public static void Pause(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        getHandler().post(new Runnable() {
            @Override
            public void run() {
                if (currVideoPlayer.exoPlayer != null) {
                    currVideoPlayer.exoPlayer.setPlayWhenReady(false);
                }
            }
        });
    }

    public static void Play(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        getHandler().post(new Runnable() {
            @Override
            public void run() {
                if (currVideoPlayer.exoPlayer != null) {
                    currVideoPlayer.exoPlayer.setPlayWhenReady(true);
                }
            }
        });
    }

    public static void SetPlaybackSpeed(final float speed, final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        getHandler().post(new Runnable() {
            @Override
            public void run() {
                if (currVideoPlayer.exoPlayer != null) {
                    PlaybackParameters param = new PlaybackParameters(speed);
                    currVideoPlayer.exoPlayer.setPlaybackParameters(param);
                }
            }
        });
    }

    public static boolean GetIsPlaying(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return false;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        return currVideoPlayer.isPlaying;
    }

    public static int GetCurrentPlaybackState(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return 0;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        return currVideoPlayer.currentPlaybackState;
    }

    public static long GetDuration(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return 0;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        return currVideoPlayer.duration;
    }

    public static int GetStereoMode(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return 0;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        return currVideoPlayer.stereoMode;
    }

    public static int GetWidth(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return 0;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        return currVideoPlayer.width;
    }

    public static int GetHeight(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return 0;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        return currVideoPlayer.height;
    }

    public static long GetPlaybackPosition(final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return 0;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);
        return Math.max(0, Math.min(currVideoPlayer.duration, currVideoPlayer.lastPlaybackPosition + (long) ((System.currentTimeMillis() - currVideoPlayer.lastPlaybackUpdateTime) * currVideoPlayer.lastPlaybackSpeed)));
    }

    public static void GetPlaybackPosition(final long position, final String videoID) {
        if (!videoPlayers.containsKey(videoID)) {
            return;
        }

        VideoPlayer currVideoPlayer = videoPlayers.get(videoID);

        getHandler().post(new Runnable() {
            @Override
            public void run() {
                if (currVideoPlayer.exoPlayer != null) {
                    Timeline timeline = currVideoPlayer.exoPlayer.getCurrentTimeline();
                    if (timeline != null) {
                        int windowIndex = timeline.getFirstWindowIndex(false);
                        long windowPositionUs = position * 1000L;
                        Timeline.Window tmpWindow = new Timeline.Window();
                        for (int i = timeline.getFirstWindowIndex(false);
                             i < timeline.getLastWindowIndex(false); i++) {
                            timeline.getWindow(i, tmpWindow);

                            if (tmpWindow.durationUs > windowPositionUs) {
                                break;
                            }

                            windowIndex++;
                            windowPositionUs -= tmpWindow.durationUs;
                        }

                        currVideoPlayer.exoPlayer.seekTo(windowIndex, windowPositionUs / 1000L);
                    }
                }
            }
        });
    }
}