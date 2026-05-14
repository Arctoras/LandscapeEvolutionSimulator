using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Assertions;

public class TerrainVisualiser
{
    ComputeShader visualiser;
    Material targetMat;

    RenderTexture texVis = null;

    int visualisationTextureKernel = -1;
    ComputeBuffer thresholdBuffer = null;
    ComputeBuffer thresholdColourBuffer = null;
    bool thresholdsChanged = false;

    public TerrainVisualiser(ComputeShader visualisationShader, Material targetMaterial)
    {
        visualiser = visualisationShader;
        targetMat = targetMaterial;
    }

    public void OnDestroy()
    {
        if(texVis) texVis.Release();
        if(thresholdBuffer != null) thresholdBuffer.Release();
        if(thresholdColourBuffer != null) thresholdColourBuffer.Release();
    }

    public void Init(Vector2Int gridDimensions)
    {
        if (texVis)
        {
            texVis.Release();
            texVis.width = gridDimensions.x;
            texVis.height = gridDimensions.y;
            texVis.Create();
        }
        else
        {

            texVis = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.sRGB);
            texVis.enableRandomWrite = true;
            texVis.Create();
            texVis.wrapMode = TextureWrapMode.Repeat;
        }

        targetMat.SetTexture("_MainTex", texVis);

        int[] dim = { gridDimensions.x, gridDimensions.y };
        visualiser.SetInts("dim", dim);
    }

    private void InsertThresholds(List<ColourThreshold> thresholds, int start, ref float[] values, ref float[] colours)
    {
        for (int t = 0; t < thresholds.Count; t++)
        {
            values[start + t] = thresholds[t].value;
            colours[4 * (start + t)] = thresholds[t].colour.r;
            colours[4 * (start + t) + 1] = thresholds[t].colour.g;
            colours[4 * (start + t) + 2] = thresholds[t].colour.b;
            colours[4 * (start + t) + 3] = thresholds[t].colour.a;
        }
    }

    public void SetThresholds(List<ColourThreshold> bedrockThresholds, List<ColourThreshold> waterThresholds)
    {
        visualiser.SetInt("bedrockThresholds", bedrockThresholds.Count);
        visualiser.SetInt("waterThresholds", waterThresholds.Count);

        int total = bedrockThresholds.Count + waterThresholds.Count;
        float[] thresholdValues = new float[total];
        float[] thresholdColours = new float[total * 4];
        int start = 0;
        InsertThresholds(bedrockThresholds, start, ref thresholdValues, ref thresholdColours);
        start += bedrockThresholds.Count;
        InsertThresholds(waterThresholds, start, ref thresholdValues, ref thresholdColours);

        if (thresholdBuffer != null) thresholdBuffer.Release(); 
        thresholdBuffer = new ComputeBuffer(total, sizeof(float), ComputeBufferType.Structured);
        thresholdBuffer.SetData(thresholdValues);
        if (thresholdColourBuffer != null) thresholdColourBuffer.Release();
        thresholdColourBuffer = new ComputeBuffer(total, 4 * sizeof(float), ComputeBufferType.Structured);
        thresholdColourBuffer.SetData(thresholdColours);
         
        thresholdsChanged = true;
    }

    public void GenerateVisTexture(Texture states, Vector3Int threadGroups)
    {
        if (visualisationTextureKernel == -1)
        {
            visualisationTextureKernel = visualiser.FindKernel("Tex");
            visualiser.SetTexture(visualisationTextureKernel, "visTexture", texVis);
        }

        if (thresholdsChanged)
        {
            visualiser.SetBuffer(visualisationTextureKernel, "thresholds", thresholdBuffer);
            visualiser.SetBuffer(visualisationTextureKernel, "colours", thresholdColourBuffer);
            thresholdsChanged = false;
        }

        visualiser.SetTexture(visualisationTextureKernel, "stateTexture", states);
        visualiser.Dispatch(visualisationTextureKernel, threadGroups.x, threadGroups.y, threadGroups.z);
    }
}

[Serializable]
public struct ColourThreshold
{
    public float value;
    public Color colour;
}