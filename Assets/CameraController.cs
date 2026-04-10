using UnityEngine;
using UnityEngine.InputSystem;

public class CameraController : MonoBehaviour
{

    [SerializeField, Min(1)] float zoom = 1;
    [SerializeField] Vector2 pos = Vector2.zero;

    new Camera camera;

    void Start()
    {
        camera = GetComponent<Camera>();
    }

    void Update()
    {
        Vector2 horizontalInput = InputSystem.actions["Move"].ReadValue<Vector2>();
        float verticalInput = InputSystem.actions["VertMove"].ReadValue<float>();
        zoom += horizontalInput.y * Time.deltaTime;
        pos += new Vector2(horizontalInput.x, verticalInput) * Time.deltaTime / zoom;
        if (zoom < 1) zoom = 1;

        if (pos.x > 1)  pos.x -= 2;
        if (pos.x < -1) pos.x += 2;
        if (pos.y > 1)  pos.y -= 2;
        if (pos.y < -1) pos.y += 2;

        transform.position = new Vector3(pos.x, pos.y, transform.position.z);
        camera.orthographicSize = Mathf.Exp(1 - zoom);
    }
}
