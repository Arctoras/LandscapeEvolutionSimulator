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
        if (texVis) texVis.Release();

        texVis = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.sRGB);
        texVis.enableRandomWrite = true;
        texVis.Create();
        texVis.wrapMode = TextureWrapMode.Repeat;
        targetMat.SetTexture("_MainTex", texVis);

        int[] dim = { gridDimensions.x, gridDimensions.y };
        visualiser.SetInts("dim", dim);
    }

    public void SetThresholds(List<ColourThreshold>[] thresholds)
    {
        Assert.IsTrue(thresholds.Length >= 3);
        int[] numThresholds = new int[3];
        int total = 0;
        for (int i = 0; i < 3; i++)
        {
            numThresholds[i] = thresholds[i].Count;
            total += thresholds[i].Count;
        }
        float[] thresholdValues = new float[total];
        float[] thresholdColours = new float[total * 4];
        int start = 0;
        for (int i = 0; i < 3; i++)
        {
            for (int t = 0; t < numThresholds[i]; t++)
            {
                thresholdValues[start + t]            = thresholds[i][t].value;
                thresholdColours[4 * (start + t)]     = thresholds[i][t].colour.r;
                thresholdColours[4 * (start + t) + 1] = thresholds[i][t].colour.g;
                thresholdColours[4 * (start + t) + 2] = thresholds[i][t].colour.b;
                thresholdColours[4 * (start + t) + 3] = thresholds[i][t].colour.a;
            }
            start += numThresholds[i];
        }

        if (thresholdBuffer != null) thresholdBuffer.Release(); 
        thresholdBuffer = new ComputeBuffer(total, sizeof(float), ComputeBufferType.Structured);
        thresholdBuffer.SetData(thresholdValues);
        if (thresholdColourBuffer != null) thresholdColourBuffer.Release();
        thresholdColourBuffer = new ComputeBuffer(total, 4 * sizeof(float), ComputeBufferType.Structured);
        thresholdColourBuffer.SetData(thresholdColours);
        visualiser.SetInts("numThresholds", numThresholds);
         
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