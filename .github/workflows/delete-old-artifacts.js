module.exports = async ({ github, context }) => {
    const fs = require('fs');
    const path = require('path');

    console.log("Script started");

    const repo = context.repo;
    console.log("Repo:", repo);

    try {
        const artifactsList = await github.rest.actions.listArtifactsForRepo({
            owner: repo.owner,
            repo: repo.repo,
        });

        console.log("Artifacts list:", artifactsList.data);

        const currentTime = new Date();
        const oneWeekAgo = new Date(currentTime.getTime() - 7 * 24 * 60 * 60 * 1000);

        for (const artifact of artifactsList.data.artifacts.slice(5)) {  // Skip the 5 most recent artifacts
            const createdAt = new Date(artifact.created_at);

            if (createdAt < oneWeekAgo) {
                console.log(`Deleting artifact ${artifact.id} created at ${artifact.created_at}`);
                await github.rest.actions.deleteArtifact({
                    owner: repo.owner,
                    repo: repo.repo,
                    artifact_id: artifact.id,
                });
            }
        }
    } catch (error) {
        console.error("Error occurred:", error);
        core.setFailed(error.message);
    }
}